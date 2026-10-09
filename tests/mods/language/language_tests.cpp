#include "bruce/mods/language/language.hpp"
#include "bruce/evidence/bundle.hpp"
#include <functional>
#include <iostream>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw bruce::Error(message);
}
bool rejects(const std::function<void()>& action) {
    try { action(); } catch (const bruce::Error&) { return true; }
    return false;
}
bruce::LanguageInputV1 input(std::string alias, std::string bytes) {
    return {std::move(alias), bytes, bruce::sha256(bytes)};
}
bruce::LanguageInputV2 input_v2(std::string alias, std::string bytes) {
    return {std::move(alias), bytes, bruce::sha256(bytes)};
}
bruce::LanguageInputV3 input_v3(std::string alias, std::string bytes) {
    return {std::move(alias), bytes, bruce::sha256(bytes)};
}
bruce::LanguageRequestV1 request(std::string text, std::string first="train", std::string second="holdout") {
    bruce::LanguageRequestV1 value;
    value.text=std::move(text);
    value.inputs={input(std::move(first),"0,0\n"),input(std::move(second),"1,1\n")};
    return value;
}
bruce::LanguageRequestV2 request_v2(std::string text, std::string first="train", std::string second="holdout") {
    bruce::LanguageRequestV2 value;
    value.text=std::move(text);
    value.inputs={input_v2(std::move(first),"0,0\n"),input_v2(std::move(second),"1,1\n")};
    return value;
}
bruce::LanguageRequestV3 request_v3(std::string text, std::string first="train", std::string second="holdout") {
    bruce::LanguageRequestV3 value;
    value.text=std::move(text);
    value.inputs={input_v3(std::move(first),"0,0\n"),input_v3(std::move(second),"1,1\n")};
    return value;
}
std::string change_v3_body(std::string bytes,std::string_view from,std::string_view to) {
    const auto header=std::string("BRUCE_LANGUAGE_DRAFT_V3\n");
    const auto body_offset=header.size()+65;
    const auto position=bytes.find(from,body_offset);
    require(position!=std::string::npos && from.size()==to.size(),"V3 mutation fixture invalid");
    bytes.replace(position,from.size(),to);
    bytes.replace(header.size(),64,bruce::sha256(std::string_view(bytes).substr(body_offset)));
    return bytes;
}
std::string swap_first_two_draft_fields(std::string bytes) {
    const auto header=std::string("BRUCE_LANGUAGE_DRAFT_V3\n");
    const auto body_offset=header.size()+65;
    const auto field_end=[&](std::size_t start) {
        const auto colon=bytes.find(':',start);
        require(colon!=std::string::npos,"V3 draft field fixture missing colon");
        return colon+1+std::stoull(bytes.substr(start,colon-start));
    };
    const auto first_end=field_end(body_offset);
    const auto second_end=field_end(first_end);
    const auto first=bytes.substr(body_offset,first_end-body_offset);
    const auto second=bytes.substr(first_end,second_end-first_end);
    bytes.replace(body_offset,second_end-body_offset,second+first);
    bytes.replace(header.size(),64,bruce::sha256(std::string_view(bytes).substr(body_offset)));
    return bytes;
}
}

int main() {
    using namespace bruce;
    try {
        const auto understood=interpret_language(request(" \tUntersuche  @train\nmit @holdout auf rationale Formeln. \r"));
        require(understood.status==LanguageInterpretationStatus::understood,"unique phrase not understood");
        require(understood.max_denominator==8,"rational portfolio default denominator changed");
        require(understood.raw_text==" \tUntersuche  @train\nmit @holdout auf rationale Formeln. \r","raw text changed");
        require(understood.normalized_text=="untersuche @train mit @holdout auf rationale formeln.","normalization changed");
        require(understood.alternatives.size()==1 && understood.alternatives[0].id=="rational-portfolio-v1","unique alternative changed");
        require(understood.selected_id.empty(),"unique phrase acquired a selection");
        const auto normalized=interpret_language(request(understood.normalized_text));
        require(normalized.normalized_text==understood.normalized_text,"normalization not idempotent");
        require(normalized.alternatives==understood.alternatives,"normalization changed meaning");

        const auto ambiguous=interpret_language(request("Untersuche @left und @right auf rationale Formeln.","left","right"));
        require(ambiguous.status==LanguageInterpretationStatus::ambiguous,"ambiguous phrase was resolved");
        require(ambiguous.alternatives.size()==2,"ambiguous alternatives lost");
        require(ambiguous.alternatives[0].id=="train=left;holdout=right","first alternative order changed");
        require(ambiguous.alternatives[1].id=="train=right;holdout=left","second alternative order changed");
        require(ambiguous.selected_id.empty(),"ambiguous phrase auto-selected");
        const auto selected=select_language_alternative(ambiguous,"train=left;holdout=right");
        require(selected.selected_id=="train=left;holdout=right","explicit selection lost");
        require(selected.alternatives==ambiguous.alternatives,"selection changed alternatives");
        require(pack_language_draft(selected)!=pack_language_draft(ambiguous),"selection not bound to serialized draft");
        require(rejects([&]{(void)select_language_alternative(ambiguous,"unknown");}),"unknown alternative accepted");
        require(rejects([&]{(void)select_language_alternative(understood,"rational-portfolio-v1");}),"understood draft accepted selection");

        const auto unsupported=interpret_language(request("Suche die beste Formel für @train."));
        require(unsupported.status==LanguageInterpretationStatus::unsupported,"unknown text inferred a task");
        require(unsupported.alternatives.empty() && unsupported.selected_id.empty(),"unsupported text acquired task");
        require(rejects([&]{(void)interpret_language(request(std::string("Untersuche ")+char(0xc3)+" @train"));}),"malformed UTF-8 accepted");
        require(rejects([&]{(void)interpret_language(request("Untersuche @train mit @holdout auf rationale Formeln.","train","other"));}),"missing alias accepted");
        require(rejects([&]{(void)interpret_language(request("Untersuche @train mit @holdout auf rationale Formeln.","train","train"));}),"duplicate alias accepted");
        auto tampered=request("Untersuche @train mit @holdout auf rationale Formeln.");
        tampered.inputs[0].bytes.push_back('x');
        require(rejects([&]{(void)interpret_language(tampered);}),"input digest tampering accepted");

        const auto packed=pack_language_draft(selected);
        const auto restored=unpack_language_draft(packed);
        require(pack_language_draft(restored)==packed,"draft bytes not canonical");
        require(restored.selected_id==selected.selected_id,"selection lost in roundtrip");
        require(rejects([&]{(void)unpack_language_draft(packed+"x");}),"trailing data accepted");
        auto digest_tampered=packed;
        digest_tampered.back()^=1;
        require(rejects([&]{(void)unpack_language_draft(digest_tampered);}),"draft digest tampering accepted");
        auto forged=selected;
        forged.inputs[0].bytes.push_back('x');
        require(rejects([&]{(void)pack_language_draft(forged);}),"forged input digest accepted");

        const auto v2a=interpret_language_v2(request_v2(
            "  Analysiere  @train anhand @holdout auf rationale Beziehungen.  "));
        const auto v2b=interpret_language_v2(request_v2(
            "Bewerte @train unter Verwendung von @holdout auf rationale Formeln."));
        const auto v2c=interpret_language_v2(request_v2(
            "Teste @train mit @holdout auf rationale Beziehungen."));
        require(v2a.grammar_version==language_grammar_version_v2 &&
                v2a.status==LanguageInterpretationStatus::understood,"V2 phrase not understood");
        require(v2a.normalized_text=="untersuche @train mit @holdout auf rationale formeln.",
                "V2 normalization changed");
        require(v2a.alternatives==v2b.alternatives && v2a.alternatives==v2c.alternatives,
                "V2 paraphrases changed meaning");
        require(v2a.inputs==v2b.inputs && v2a.inputs==v2c.inputs,
                "V2 paraphrases changed bindings");
        const auto v2_ambiguous=interpret_language_v2(request_v2(
            "Analysiere @left und @right auf rationale Formeln.","left","right"));
        require(v2_ambiguous.status==LanguageInterpretationStatus::ambiguous &&
                v2_ambiguous.alternatives.size()==2,"V2 ambiguity not visible");
        require(v2_ambiguous.alternatives[0].id=="train=left;holdout=right" &&
                v2_ambiguous.alternatives[1].id=="train=right;holdout=left",
                "V2 ambiguity order changed");
        const auto v2_selected=select_language_alternative_v2(v2_ambiguous,"train=right;holdout=left");
        require(v2_selected.selected_id=="train=right;holdout=left","V2 selection lost");
        require(rejects([&]{(void)select_language_alternative_v2(v2_ambiguous,"unknown");}),
                "V2 unknown alternative accepted");
        const auto v2_packed=pack_language_draft_v2(v2_selected);
        require(v2_packed.starts_with("BRUCE_LANGUAGE_DRAFT_V2\n"),"V2 header missing");
        require(pack_language_draft_v2(unpack_language_draft_v2(v2_packed))==v2_packed,
                "V2 draft bytes not canonical");
        const auto v2_unsupported=interpret_language_v2(request_v2("Finde die beste Formel."));
        require(v2_unsupported.status==LanguageInterpretationStatus::unsupported,
                "V2 unsupported text inferred a task");
        require(rejects([&]{(void)pack_language_draft_v2(v2_unsupported);}),
                "V2 unsupported draft was exportable");
        require(rejects([&]{(void)interpret_language_v2(request_v2(
            "Analysiere @train mit @holdout auf rationale Formeln.","train","other"));}),
                "V2 missing alias accepted");
        auto v2_forged=v2_selected;
        v2_forged.grammar_version="future";
        require(rejects([&]{(void)pack_language_draft_v2(v2_forged);}),
                "V2 grammar tampering accepted");

        const std::string v3_raw="  Prüfe  @train\ngegen @holdout auf rationale Formeln.  ";
        const auto v3=interpret_language_v3(request_v3(v3_raw));
        require(v3.grammar_version==language_grammar_version_v3 &&
                v3.status==LanguageInterpretationStatus::understood,
                "V3 approved paraphrase not understood");
        require(v3.raw_text==v3_raw &&
                v3.normalized_text=="untersuche @train mit @holdout auf rationale formeln.",
                "V3 raw text or canonical normalization changed");
        require(v3.alternatives.size()==1 &&
                v3.alternatives[0].id=="rational-portfolio-v2" &&
                v3.alternatives[0].rule=="rational-portfolio-v2 train=train holdout=holdout",
                "V3 introduced a new meaning");
        const auto v3_canonical=interpret_language_v3(request_v3(v3.normalized_text));
        require(v3_canonical.normalized_text==v3.normalized_text &&
                v3_canonical.alternatives==v3.alternatives,
                "V3 normalization not idempotent");
        const auto v3_v2_form=interpret_language_v3(request_v3(
            "Analysiere @train anhand @holdout auf rationale Beziehungen."));
        require(v3_v2_form.normalized_text==v3.normalized_text &&
                v3_v2_form.alternatives==v3.alternatives,
                "V3 changed the existing paraphrase meaning");

        const auto v3_ambiguous=interpret_language_v3(request_v3(
            "Vergleiche @right und @left auf rationale Formeln.","right","left"));
        require(v3_ambiguous.status==LanguageInterpretationStatus::ambiguous &&
                v3_ambiguous.selected_id.empty() && v3_ambiguous.alternatives.size()==2,
                "V3 ambiguity was resolved without selection");
        require(v3_ambiguous.alternatives[0].id=="train=left;holdout=right" &&
                v3_ambiguous.alternatives[1].id=="train=right;holdout=left",
                "V3 alternatives lost canonical order");
        const auto v3_selected=select_language_alternative_v3(v3_ambiguous,"train=right;holdout=left");
        require(v3_selected.selected_id=="train=right;holdout=left" &&
                v3_selected.alternatives==v3_ambiguous.alternatives,
                "V3 explicit selection changed interpretation");
        require(rejects([&]{(void)select_language_alternative_v3(v3_ambiguous,"unknown");}),
                "V3 unknown alternative accepted");
        require(rejects([&]{(void)select_language_alternative_v3(v3,"rational-portfolio-v2");}),
                "V3 understood draft accepted selection");

        const auto v3_unsupported=interpret_language_v3(request_v3("Finde die beste Formel."));
        require(v3_unsupported.status==LanguageInterpretationStatus::unsupported &&
                v3_unsupported.alternatives.empty() && v3_unsupported.selected_id.empty(),
                "V3 unknown text inferred a task");
        require(rejects([&]{(void)interpret_language_v3(request_v3(
            std::string("Prüfe ")+char(0xc3)+" @train"));}),
            "V3 malformed UTF-8 accepted");
        require(rejects([&]{(void)interpret_language_v3(request_v3(
            "Prüfe @train gegen @holdout auf rationale Formeln.","train","other"));}),
            "V3 missing alias accepted");
        require(rejects([&]{(void)interpret_language_v3(request_v3(
            "Prüfe @train gegen @holdout auf rationale Formeln.","train","train"));}),
            "V3 duplicate alias accepted");
        auto v3_tampered=request_v3("Prüfe @train gegen @holdout auf rationale Formeln.");
        v3_tampered.inputs[0].bytes.push_back('x');
        require(rejects([&]{(void)interpret_language_v3(v3_tampered);}),
                "V3 input digest tampering accepted");

        const auto v3_bytes=pack_language_draft_v3(v3);
        require(v3_bytes.starts_with("BRUCE_LANGUAGE_DRAFT_V3\n"),"V3 draft header missing");
        const auto v3_restored=unpack_language_draft_v3(v3_bytes);
        require(pack_language_draft_v3(v3_restored)==v3_bytes &&
                v3_restored.raw_text==v3_raw && v3_restored.normalized_text==v3.normalized_text &&
                v3_restored.inputs==v3.inputs && v3_restored.budget.candidates==v3.budget.candidates,
                "V3 draft replay changed bound fields");
        require(pack_language_draft_v3(v3)==v3_bytes,"V3 draft bytes are not deterministic");
        const auto v3_selected_bytes=pack_language_draft_v3(v3_selected);
        require(unpack_language_draft_v3(v3_selected_bytes).selected_id==v3_selected.selected_id,
                "V3 explicit selection lost in replay");
        require(rejects([&]{(void)pack_language_draft_v3(v3_unsupported);}),
                "V3 unsupported draft exported");
        require(rejects([&]{(void)pack_language_draft_v3(v3_ambiguous);}),
                "V3 unselected draft exported");
        auto v3_forged=v3;
        v3_forged.grammar_version="future";
        require(rejects([&]{(void)pack_language_draft_v3(v3_forged);}),
                "V3 future grammar exported");
        v3_forged=v3; v3_forged.normalized_text="wrong";
        require(rejects([&]{(void)pack_language_draft_v3(v3_forged);}),
                "V3 noncanonical normalization exported");
        v3_forged=v3; v3_forged.inputs[0].digest[0]^=1;
        require(rejects([&]{(void)pack_language_draft_v3(v3_forged);}),
                "V3 mutated input digest exported");
        v3_forged=v3_selected; v3_forged.selected_id="unknown";
        require(rejects([&]{(void)pack_language_draft_v3(v3_forged);}),
                "V3 unknown selection exported");
        require(rejects([&]{(void)unpack_language_draft_v3(v3_bytes+"x");}),
                "V3 trailing draft bytes accepted");
        require(rejects([&]{(void)unpack_language_draft_v3(v2_packed);}),
                "V2 draft accepted as V3");
        require(rejects([&]{(void)unpack_language_draft_v2(v3_bytes);}),
                "V3 draft accepted as V2");
        require(rejects([&]{(void)unpack_language_draft_v3(packed);}),
                "V1 draft accepted as V3");
        require(rejects([&]{(void)unpack_language_draft(v3_bytes);}),
                "V3 draft accepted as V1");
        require(rejects([&]{(void)unpack_language_draft_v3(swap_first_two_draft_fields(v3_bytes));}),
                "V3 rehashed field-order mutation accepted");
        require(rejects([&]{(void)unpack_language_draft_v3(change_v3_body(
            v3_bytes,"language-grammar-v3","language-grammar-v4"));}),
            "V3 unknown grammar version accepted");
        require(rejects([&]{(void)unpack_language_draft_v3(change_v3_body(
            v3_bytes,"untersuche @train","untersuche @other"));}),
            "V3 noncanonical normalization accepted");
        require(rejects([&]{(void)unpack_language_draft_v3(change_v3_body(
            v3_selected_bytes,"train=right;holdout=left","train=wrong;holdout=left"));}),
            "V3 invalid selection accepted after rehash");
        require(rejects([&]{(void)unpack_language_draft_v3(change_v3_body(
            v3_bytes,v3.inputs[0].digest,std::string(64,'0')));}),
            "V3 input digest mutation accepted after outer rehash");
        auto v3_extra=v3_bytes+"1:x";
        v3_extra.replace(std::string("BRUCE_LANGUAGE_DRAFT_V3\n").size(),64,
            sha256(std::string_view(v3_extra).substr(std::string("BRUCE_LANGUAGE_DRAFT_V3\n").size()+65)));
        require(rejects([&]{(void)unpack_language_draft_v3(v3_extra);}),
                "V3 unknown or duplicate trailing field accepted");
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
