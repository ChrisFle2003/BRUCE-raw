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
bruce::LanguageRequestV1 request(std::string text, std::string first="train", std::string second="holdout") {
    bruce::LanguageRequestV1 value;
    value.text=std::move(text);
    value.inputs={input(std::move(first),"0,0\n"),input(std::move(second),"1,1\n")};
    return value;
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
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
