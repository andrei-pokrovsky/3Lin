#pragma once
#include "bpe.h"
#include "ppm_window.h"
#include <gpt/rng/xrng.h>


void GenerateArithmetic();
void GenerateArithmetic97();


///////////////////////////////////////////////////////////////////////////////////////////////////
void LoadDocument(TVector<char> *pRes, const TString &fileName);
void LoadDocumentSetFromFiles(TVector<TVector<char>> *pRes, const TString &dir);
void LoadDocumentSetFromBin(TVector<TVector<char>> *pRes, const TString &fileName);
void LoadTokenized(const TString &fileName, yint tokenWidth, TVector<TBPEToken> *p);
void SaveDocumentSetToBin(const TVector<TVector<char>> &textArr, const TString &fileName);


///////////////////////////////////////////////////////////////////////////////////////////////////
struct TFragment
{
    yint Offset = 0;
    TVector<TBPEToken> Text;
    TVector<TBPEToken> PPM1;
    TVector<TBPEToken> Target;
    SAVELOAD(Offset, Text, PPM1, Target);

    yint GetLength() const { return YSize(Text); }
};


///////////////////////////////////////////////////////////////////////////////////////////////////
class TFragmentGen
{
    bool UsePPM = false;
    TVector<TBPEToken> Text;
    TVector<TBPEToken> PPM;
    TWindowPPMIndex Index;
public:
    TFragmentGen(bool usePPM) : UsePPM(usePPM) {}

    void AddToken(TBPEToken token)
    {
        Text.push_back(token);
        if (UsePPM) {
            yint bestLen = 0;
            yint bestPos = 0;
            Index.IndexPos(Text, YSize(Text) - 1, &bestLen, &bestPos);
            if (bestLen > 0) {
                PPM.push_back(Text[bestPos + 1]);
            } else {
                PPM.push_back(UNDEFINED_TOKEN);
            }
        }
    }

    void FillFragment(TFragment *pFrag, yint maxLen) const
    {
        *pFrag = TFragment();
        yint start = Max<yint>(0, YSize(Text) - maxLen);
        yint fin = YSize(Text);
        for (yint t = start; t < fin; ++t) {
            pFrag->Text.push_back(Text[t]);
            if (UsePPM) {
                pFrag->PPM1.push_back(PPM[t]);
            }
        }
    }
};


///////////////////////////////////////////////////////////////////////////////////////////////////
struct TDatasetWeightedSpan
{
    double Weight = 0;
    yint DocsetId = 0;
    yint SpanStart = 0;
    yint SpanFinish = 0;

    TDatasetWeightedSpan() {}
    TDatasetWeightedSpan(double w, yint id, yint start, yint finish) : Weight(w), DocsetId(id), SpanStart(start), SpanFinish(finish) {}
};


struct TDatasetParams
{
    TVector<double> FreqArr;
    yint TotalUtf8Chars = 0;
    yint TotalTokens = 0;
    yint BytesPerToken = 0;
    TVector<TDatasetWeightedSpan> TrainSpans;
    TVector<TDatasetWeightedSpan> TestSpans;
    SAVELOAD(FreqArr, TotalUtf8Chars, TotalTokens, BytesPerToken, TrainSpans, TestSpans);

    TDatasetParams() {}
    TDatasetParams(yint vocabSize)
    {
        ClearPodArray(&FreqArr, vocabSize);
        BytesPerToken = (vocabSize > 65530) ? 3 : 2;
    }
    void CountDocset(const TVector<TBPEToken> &data, yint offset, yint utf8charCount, float testFraction)
    {
        yint vocabSize = YSize(FreqArr);
        for (ui64 x : data) {
            Y_VERIFY(x < vocabSize);
            FreqArr[x] += 1;
        }
        yint len = YSize(data);
        if (testFraction == 0) {
            TrainSpans.push_back(TDatasetWeightedSpan(len, -1, offset, offset + len));
        } else if (testFraction == 1) {
            TestSpans.push_back(TDatasetWeightedSpan(len, -1, offset, offset + len));
        } else {
            // test is always last part of docset
            yint testLen = len * testFraction;
            yint trainLen = len - testLen;
            TrainSpans.push_back(TDatasetWeightedSpan(trainLen, -1, offset, offset + trainLen));
            TestSpans.push_back(TDatasetWeightedSpan(testLen, -1, offset + trainLen, offset + len));
        }
        TotalUtf8Chars += utf8charCount;
        TotalTokens += len;
    }
};


class TDataset
{
    struct TDocumentSet
    {
        TVector<TBPEToken> Text;
        TVector<TBPEToken> PPM;
        TString IndexFilename;
        TString PPMIndexFilename;
        yint BytesPerToken = 0;
        SAVELOAD(Text, PPM, IndexFilename, PPMIndexFilename, BytesPerToken);
        TIntrusivePtr<TPackedBPETokenReader> Reader;
        TIntrusivePtr<TPackedBPETokenReader> PPMReader;

        void FillFragment(bool usePPM, yint offset, yint fragLen, TFragment *p)
        {
            *p = TFragment();
            p->Offset = offset;
            if (IndexFilename.empty()) {
                for (yint t = 0; t < fragLen; ++t) {
                    p->Text.push_back(Text[offset + t]);
                    if (usePPM) {
                        p->PPM1.push_back(PPM[offset + t]);
                    }
                    p->Target.push_back(Text[offset + t + 1]);
                }
            } else {
                if (Reader.Get() == 0) {
                    Reader = new TPackedBPETokenReader(IndexFilename, BytesPerToken);
                    if (!PPMIndexFilename.empty()) {
                        PPMReader = new TPackedBPETokenReader(PPMIndexFilename, BytesPerToken);
                    }
                }
                TVector<TBPEToken> buf;
                Reader->Read(offset, fragLen + 1, &buf);
                for (yint t = 0; t < fragLen; ++t) {
                    p->Text.push_back(buf[t]);
                    p->Target.push_back(buf[t + 1]);
                }
                if (usePPM) {
                    PPMReader->Read(offset, fragLen, &p->PPM1);
                }
            }
        }
    };

    TVector<TDocumentSet> DocsetArr;
    TVector<float> BiasArr;
    bool UsePPM = false;
    yint VocabSize = 0;
    float Compression = 0;
    TVector<TDatasetWeightedSpan> TrainSpans;
    TVector<TDatasetWeightedSpan> TestSpans;
    TVector<TFragment> BertFragments;
public:
    SAVELOAD(DocsetArr, BiasArr, UsePPM, VocabSize, Compression, TrainSpans, TestSpans, BertFragments);

private:
    template <class TRng>
    void MakeRandomFragment(TRng &rng,
        yint docsetId, yint spanStart, yint spanFinish,
        yint fragLen, TFragment *p)
    {
        TDocumentSet &docset = DocsetArr[docsetId];
        if (spanFinish - spanStart <= fragLen - 1) {
            docset.FillFragment(UsePPM, spanStart, spanFinish - spanStart - 1, p);
        } else {
            yint offset = spanStart + rng.Uniform(spanFinish - spanStart - fragLen - 1);
            docset.FillFragment(UsePPM, offset, fragLen, p);
        }
    }

public:
    enum ETrainTest
    {
        TRAIN,
        TEST,
    };

    float GetCompression() const
    {
        return Compression;
    }

    yint GetVocabSize() const
    {
        return VocabSize;
    }

    const TVector<float> &GetBias() const
    {
        return BiasArr;
    }

    bool HasTest() const
    {
        return !TestSpans.empty();
    }

    void MakeFragment(ETrainTest trt, TXRng &rng, yint len, TFragment *pFrag)
    {
        if (!BertFragments.empty()) {
            *pFrag = BertFragments[rng.Uniform(YSize(BertFragments))];
        } else {
            const TVector<TDatasetWeightedSpan> &spanArr = (trt == TRAIN) ? TrainSpans : TestSpans;
            Y_VERIFY(!spanArr.empty());
            // use gumbel max trick
            float best = -1e38f;
            const TDatasetWeightedSpan *bestSpan = &spanArr[0];
            for (yint k = 0; k < YSize(spanArr); ++k) {
                float score = spanArr[k].Weight / -log(rng.GenRandReal3());
                if (score > best) {
                    best = score;
                    bestSpan = &spanArr[k];
                }
            }
            MakeRandomFragment(rng, bestSpan->DocsetId, bestSpan->SpanStart, bestSpan->SpanFinish, len, pFrag);
        }
    }

    void LoadBert(yint vocabSize);

    friend class TDatasetBuilder;
};


class TDatasetBuilder : public TThrRefBase
{
    TDataset &Dataset;
    TVector<double> FreqArr;
    yint TotalUtf8Chars = 0;
    yint TotalTokens = 0;
    yint DocStartToken = -1;

private:
    void Init(bool usePPM, yint vocabSize, yint docStartToken)
    {
        DocStartToken = docStartToken;
        Dataset = TDataset();
        Dataset.UsePPM = usePPM;
        Dataset.VocabSize = vocabSize;
        ClearPodArray(&FreqArr, vocabSize);
        Dataset.DocsetArr.reserve(10000);
    }

    void AddParams(yint docsetId, const TDatasetParams &params, float weight)
    {
        Y_VERIFY(YSize(FreqArr) == YSize(params.FreqArr));
        for (yint x = 0; x < YSize(params.FreqArr); ++x) {
            FreqArr[x] += params.FreqArr[x];
        }
        for (const TDatasetWeightedSpan &span : params.TrainSpans) {
            Dataset.TrainSpans.push_back(TDatasetWeightedSpan(span.Weight * weight, docsetId, span.SpanStart, span.SpanFinish));
        }
        for (const TDatasetWeightedSpan &span : params.TestSpans) {
            Dataset.TestSpans.push_back(TDatasetWeightedSpan(span.Weight * weight, docsetId, span.SpanStart, span.SpanFinish));
        }
        TotalUtf8Chars += params.TotalUtf8Chars;
        TotalTokens += params.TotalTokens;
    }

public:
    TDatasetBuilder(TDataset *pDataset, bool usePPM, yint vocabSize, yint docStartToken) : Dataset(*pDataset)
    {
        Init(usePPM, vocabSize, docStartToken);
    }

    TDatasetBuilder(TDataset *pDataset, bool usePPM, const TTokenizer &tokenizer) : Dataset(*pDataset)
    {
        yint docStartToken = tokenizer.HasDocStartToken() ? tokenizer.GetDocStartToken() : -1;
        Init(usePPM, tokenizer.GetVocabSize(), docStartToken);
    }

    ~TDatasetBuilder()
    {
        yint vocabSize = Dataset.VocabSize;
        Dataset.BiasArr.resize(vocabSize);
        for (yint c = 0; c < vocabSize; ++c) {
            Dataset.BiasArr[c] = log2(FreqArr[c] + 0.5);
        }
        Dataset.Compression = TotalTokens / (TotalUtf8Chars + 0.);
    }

    void AddTokenizedDocset(const TVector<TBPEToken> &data, const TDatasetParams &params, float weight)
    {
        yint docsetId = YSize(Dataset.DocsetArr);
        TDataset::TDocumentSet &docset = *Dataset.DocsetArr.insert(Dataset.DocsetArr.end());
        docset.Text = data;
        if (Dataset.UsePPM) {
            ComputeWindowPPM(docset.Text, &docset.PPM, DocStartToken);
        }
        AddParams(docsetId, params, weight);
    }

    void AddIndexedDocset(const TString &indexFilename, const TString &ppmIndexFilename, const TDatasetParams &params, yint vocabSize, float weight)
    {
        Y_VERIFY(vocabSize == Dataset.VocabSize);
        yint docsetId = YSize(Dataset.DocsetArr);
        TDataset::TDocumentSet &docset = *Dataset.DocsetArr.insert(Dataset.DocsetArr.end());
        docset.IndexFilename = indexFilename;
        if (Dataset.UsePPM) {
            docset.PPMIndexFilename = ppmIndexFilename;
        }
        docset.BytesPerToken = params.BytesPerToken;
        AddParams(docsetId, params, weight);
    }
};


///////////////////////////////////////////////////////////////////////////////////////////////////
void MakeCharDataset(TDataset *pDataset, TTokenizer *pTokenizer, const TVector<char> &text, float testFraction, bool usePPM);
void AddDocset(TIntrusivePtr<TDatasetBuilder> pBuilder, const TTokenizer &tokenizer, const TVector<TVector<char>> &docSet, float weight, float testFraction);

void AddIndexedDocset(TDatasetBuilder *pBuilder, const TString &dir, float weight);
void IndexDocsetDir(const TString &dir, const TTokenizer &tokenizer, bool usePPM, float testFraction);
