#pragma once
#include "att.h"

typedef ui32 TLabelIndex;
const TLabelIndex INVALID_LABEL_INDEX = 0xffffffff;

struct TNodeTarget
{
    yint Node = 0;
    yint TargetId = 0;

    TNodeTarget() {}
    TNodeTarget(yint nodeId, yint targetId) : Node(nodeId), TargetId(targetId) {}
};

inline bool operator==(const TNodeTarget &a, const TNodeTarget &b)
{
    return a.Node == b.Node && a.TargetId == b.TargetId;
}


struct TNodesBatch
{
    TVector<TLabelIndex> LabelArr;
    TVector<ui32> LabelPtr;
    TVector<int> SampleIndex;
    TVector<TNodeTarget> Target;
    TAttentionInfo Att;
    TAttentionInfo WideAtt;

    void Init();
    void AddSample(int idx, const TVector<TLabelIndex> &labels, const TVector<TAttentionSpan> &attSpans, const TVector<TAttentionSpan> &wideAttSpans);
    yint GetNodeCount() const { return YSize(SampleIndex); }
};

