#include "stdafx.h"
#include "sample_model.h"
#include <lib/net/http_server.h>
#include <lib/net/http_request.h>
#include <lib/net/html_compose.h>
#include <util/string.h>


using namespace NNet;

struct TContState
{
    TString Prompt;
    TString Cont;
    bool Finished = true;
};


struct TStateXML
{
    TString XML;

    void Render(const TContState &cs)
    {
        XML = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        XML += "<root>\n";
        XML += Sprintf("<Finished>%g</Finished>", cs.Finished ? 1. : 0.);

        XML += Sprintf("<Data><id>cont</id><val>%s</val></Data>\n",
            EncodeXML(cs.Cont).c_str());
            //EncodeXML(Win2Utf(cs.Cont).c_str())).c_str());
        XML += "</root>\n";
    }
};



static void RenderRootPage(TString *pRes, const TString &modelName)
{
    TString cssStyles =
        "table {border-collapse: collapse;}\n"
        "td {padding:0.5rem;}\n"
        "tr,td{text-align:left;vertical-align:top;}\n"
        ;
    NNet::THtmlPage page(Sprintf("Continuation using model %s", modelName.c_str()), cssStyles, "");
    page +=
        "<script>\n"
        "function ApplyChanges(xmlDoc) {\n"
        "  var x = xmlDoc.getElementsByTagName('Data');\n"
        "  for (i = 0; i < x.length; i++) {\n"
        "    var id = x[i].childNodes[0].childNodes[0].nodeValue;\n"
        "    var val = x[i].childNodes[1].childNodes[0].nodeValue;\n"
        "    elem = document.getElementById(id);\n"
        "    elem.value = val;\n"
        "  }\n"
        "}\n"
        "var xquery;\n"
        "function LoadCont() {\n"
        "  if (xquery) { xquery.onreadystatechange = function(){}; xquery.abort(); }\n"
        "  xquery = new XMLHttpRequest();\n"
        "  xquery.onreadystatechange = function() {\n"
        "    if (this.readyState == 4) {\n"
        "      if (this.status == 200) {\n"
        "        var xmlDoc = this.responseXML;\n"
        "        ApplyChanges(xmlDoc);\n"
        "        var finishElem = xmlDoc.getElementsByTagName('Finished')[0];\n"
        "        var finFlag = finishElem.childNodes[0].nodeValue;\n"
        "        if (finFlag == 0) {\n"
        "          LoadCont();\n"
        "        }\n"
        "        document.getElementById('sstate').innerHTML = '';\n"
        "      } else {\n"
        "        document.getElementById('sstate').innerHTML = 'ATTENTION, server is down<br>';\n"
        "      }\n"
        "    }\n"
        "  };\n"
        "  var prompt = document.getElementById('prompt');\n"
        "  var cont = document.getElementById('cont');\n"
        "  xquery.open('GET', encodeURI('cont?prompt=' + prompt.value + '&cont=' + cont.value));\n"
        "  xquery.send();\n"
        "}\n"
        "function Run() {\n"
        "  var cont = document.getElementById('cont');\n"
        "  cont.value = '';\n"
        "  LoadCont();\n"
        "}\n"
        "</script>\n"
        ;

    page += "<div id='sstate'></div>";

    page +=
        "<table>\n"
        "<tr><td><textarea id='prompt' rows='20' cols='80'>Сегодня самый лучший день </textarea>\n"
        "<tr><td><textarea id='cont' rows='20' cols='80' disabled='disabled'></textarea>\n"
        "<tr><td><button onclick='Run()' style='font-size:large'>Run</button>\n"
        "</table>";

    page.MakeHtml(pRes);
}


int main()
{
#ifdef _MSC_VER
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    TXRng rng(GetCycleCount());

    const TString tokenizerFilename = "d:/tokenizers/50k.bin";
    const TString modelFilename = "D:/models/rus_big/eden_gpt_274k.bin";

    TTokenizer tokenizer;
    Serialize(true, tokenizerFilename, tokenizer);

    TSamplingModel model;
    {
        TModelParams modelParams;
        Serialize(true, modelFilename, modelParams);
        model.Init(modelParams, tokenizer);
    }


    // serve queries
    THttpServer srv(11311);
    DebugPrintf("start serving queries\n");
    for (;;) {
        THttpRequest req;
        if (!srv.CanAccept(0.1f)) {
            continue;
        }
        SOCKET s = srv.AcceptNonBlocking(&req);
        if (s == INVALID_SOCKET) {
            continue;
        }
        if (req.Req == "") {
            TString html;
            RenderRootPage(&html, modelFilename);
            HttpReplyHTML(s, html);
        } else if (req.Req == "cont") {
            TContState cs;
            cs.Prompt = DecodeCGI(req.GetParam("prompt"));
            cs.Cont = DecodeCGI(req.GetParam("cont"));
            TString next = SampleFromModel(rng, model, cs.Prompt + cs.Cont);
            cs.Finished = next.empty(); // stop if EOT was generated
            cs.Cont += next;
            TStateXML xml;
            xml.Render(cs);
            HttpReplyXML(s, xml.XML);
            //DebugPrintf("query prompt %s, cont %s\n", cs.Prompt.c_str(), cs.Cont.c_str());
        } else {
            ReplyNotFound(s);
        }
    }
    return 0;
}
