#include "bruce/application/fraction_compare.hpp"
#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
#include <sstream>

namespace bruce {
namespace {
constexpr std::size_t serialization_block_size=64*1024;
void append_chunked(std::ostringstream& out,std::string_view text,const ReplayCheckpoint& records) {
    for(std::size_t offset=0;offset<text.size();) {
        if(records) records();
        const auto count=std::min(serialization_block_size,text.size()-offset);
        out.write(text.data()+offset,static_cast<std::streamsize>(count));
        offset+=count;
    }
}
std::string_view engine(const FractionComparisonTask& task) {
    if(task.domain_policy=="modular-witness") return "bruce-fraction-comparison-v3";
    return task.domain_policy=="modular" ? "bruce-fraction-comparison-v2" : "bruce-fraction-comparison-v1";
}
std::string_view policy(const FractionComparisonTask& task) {
    if(task.domain_policy=="modular-witness") return "rational-domain-gcd-modular-witness16-v1";
    return task.domain_policy=="modular" ? "rational-domain-gcd-modular-v1" : "rational-domain-gcd-quadratic-v1";
}
std::string_view header(const FractionComparisonTask& task) {
    if(task.domain_policy=="modular-witness") return "BRUCE_FRACTION_COMPARISON_V3\n";
    return task.domain_policy=="modular" ? "BRUCE_FRACTION_COMPARISON_V2\n" : "BRUCE_FRACTION_COMPARISON_V1\n";
}
struct ComparisonStop : Error {using Error::Error;};
using Poly=std::vector<ExactValue>;
Poly parse_polynomial(std::string_view text,const NumberDomain& domain) {
    if(text.size()>1024*1024) throw Error("PolynomialInputByteLimit");
    Poly result;
    while(true) {
        if(result.size()==65) throw ExpressionNormalizationLimit{};
        const auto comma=text.find(',');
        result.push_back(domain.parse(text.substr(0,comma)));
        if(comma==std::string_view::npos) break;
        text.remove_prefix(comma+1);
    }
    return result;
}
std::string coefficient_text(const Poly& values) {
    std::string text;
    for(const auto& v:values) {if(!text.empty()) text+=',';text+=exact_text(v);}
    return text;
}
std::string array(const Poly& values) {
    std::string text="[";
    for(const auto& v:values) {if(text.size()>1) text+=',';text+=json_string(exact_text(v));}
    return text+"]";
}
std::string form_json(const std::optional<RationalExpressionForm>& form) {
    if(!form) return "null";
    return "{\"numerator\":"+array(form->numerator)+",\"denominator\":"+array(form->denominator)+
        ",\"excluded_polynomial\":"+array(form->excluded_polynomial)+",\"empty_domain\":"+(form->empty_domain ? "true" : "false")+"}";
}
std::string task_text(const FractionComparisonTask& task) {
    return "engine "+std::string(engine(task))+"\npolicy "+std::string(policy(task))+"\nmax_bits "+std::to_string(task.max_bits)+
        "\nmax_operations "+std::to_string(task.max_operations)+"\ncancel "+
        (task.cancel_after_operations ? std::to_string(*task.cancel_after_operations) : "none")+
        "\nleft_numerator "+task.left_numerator+"\nleft_denominator "+task.left_denominator+
        "\nright_numerator "+task.right_numerator+"\nright_denominator "+task.right_denominator+"\n";
}
std::string field(std::istream& stream,std::string_view name) {
    std::string key,value,extra,line;
    if(!std::getline(stream,line)) throw Error("TruncatedBundle");
    std::istringstream row(line);
    if(!(row>>key>>value) || key!=name || (row>>extra)) throw Error("InvalidBundleField: "+std::string(name));
    return value;
}
}
FractionComparisonRun compare_fractions(FractionComparisonTask task,const std::function<bool(std::uint64_t)>& cancelled) {
    if(task.domain_policy!="quadratic" && task.domain_policy!="modular" && task.domain_policy!="modular-witness") throw Error("InvalidDomainPolicy");
    const NumberDomain d("rational",task.max_bits);
    if(!task.max_operations || task.max_operations>1000000) throw Error("InvalidComparisonOperationBudget: expected 1..1000000");
    if(task.cancel_after_operations && *task.cancel_after_operations>task.max_operations) throw Error("InvalidComparisonCancellation");
    const auto ln=parse_polynomial(task.left_numerator,d),ld=parse_polynomial(task.left_denominator,d),
        rn=parse_polynomial(task.right_numerator,d),rd=parse_polynomial(task.right_denominator,d);
    task.left_numerator=coefficient_text(ln);task.left_denominator=coefficient_text(ld);
    task.right_numerator=coefficient_text(rn);task.right_denominator=coefficient_text(rd);
    FractionComparisonRun run;run.task=std::move(task);
    const ExpressionSpend spend=[&](std::uint64_t amount) {
        if((run.task.cancel_after_operations && run.operations>=*run.task.cancel_after_operations) ||
            (cancelled && cancelled(run.operations))) {
            run.task.cancel_after_operations=run.operations;
            throw ComparisonStop("Cancelled");
        }
        if(amount>run.task.max_operations-run.operations) throw ComparisonStop("OperationBudgetExceeded");
        run.operations+=amount;
    };
    try {
        run.left=normalize_fraction(d,ln,ld,spend);run.trace.push_back("LEFT_NORMALIZED");
        run.right=normalize_fraction(d,rn,rd,spend);run.trace.push_back("RIGHT_NORMALIZED");
        run.domains=compare_rational_domains(d,run.left->excluded_polynomial,run.right->excluded_polynomial,spend,run.task.domain_policy!="quadratic",run.task.domain_policy=="modular-witness");
        run.trace.push_back("DOMAINS_"+run.domains.relation);
        if(run.left->empty_domain || run.right->empty_domain) {
            run.value_relation="NOT_APPLICABLE";
        } else {
            auto equal=[&](const Poly& a,const Poly& b) {
                spend(1);if(a.size()!=b.size()) return false;
                for(std::size_t i=0;i<a.size();++i) {spend(1);if(a[i]!=b[i]) return false;}
                return true;
            };
            const bool numerator_equal=equal(run.left->numerator,run.right->numerator);
            const bool denominator_equal=equal(run.left->denominator,run.right->denominator);
            run.value_relation=numerator_equal && denominator_equal ? "EQUAL" : "DIFFERENT";
        }
        spend(1);run.trace.push_back("VALUES_"+run.value_relation);
        if(run.domains.relation=="DIFFERENT" || run.value_relation=="DIFFERENT") run.status="PROVEN_DIFFERENT";
        else if(run.domains.relation=="EQUAL") run.status="PROVEN_EQUIVALENT";
        else run.status="UNDECIDED";
    } catch(const ComparisonStop& error) {run.stop_reason=error.what();}
      catch(const NumericLimit& error) {run.stop_reason=error.what();}
      catch(const ExpressionNormalizationLimit& error) {run.stop_reason=error.what();}
    return run;
}
std::string render_fraction_comparison(const FractionComparisonRun& run) {
    std::string out="{\"engine\":"+json_string(engine(run.task))+",\"policy\":"+json_string(policy(run.task))+
        ",\"input_domain\":\"rational\",\"proof_scope\":\"supplied-polynomial-fractions-over-Q\",\"status\":"+json_string(run.status)+
        ",\"stop_reason\":"+json_string(run.stop_reason)+",\"operations\":"+std::to_string(run.operations)+
        ",\"max_operations\":"+std::to_string(run.task.max_operations)+",\"max_bits\":"+std::to_string(run.task.max_bits)+
        ",\"value_relation\":"+json_string(run.value_relation)+",\"domain_relation\":"+json_string(run.domains.relation)+
        ",\"left\":"+form_json(run.left)+",\"right\":"+form_json(run.right)+
        ",\"domain_certificate\":{\"reason\":"+json_string(run.domains.reason)+
        ",\"left_remainder\":"+array(run.domains.left_remainder)+",\"right_remainder\":"+array(run.domains.right_remainder)+
        ",\"left_roots\":"+json_string(run.domains.left_roots)+",\"right_roots\":"+json_string(run.domains.right_roots)+
        ",\"witness\":"+(run.domains.witness ? json_string(exact_text(*run.domains.witness)) : "null")+
        ",\"excluded_side\":"+json_string(run.domains.excluded_side);
    if(run.task.domain_policy!="quadratic") {
        const auto residues=[](const std::vector<unsigned>& values) {
            std::string out="[";
            for(const auto value:values) {if(out.size()>1) out+=',';out+=std::to_string(value);}
            return out+"]";
        };
        out+=",\"left_modular_prime\":"+std::to_string(run.domains.left_modular_prime)+
            ",\"right_modular_prime\":"+std::to_string(run.domains.right_modular_prime)+
            ",\"left_modular_values\":"+residues(run.domains.left_modular_values)+
            ",\"right_modular_values\":"+residues(run.domains.right_modular_values);
    }
    if(run.task.domain_policy=="modular-witness") {
        out+=",\"left_witness_candidates\":"+std::to_string(run.domains.left_witness_candidates)+
            ",\"right_witness_candidates\":"+std::to_string(run.domains.right_witness_candidates);
    }
    out+="},\"trace\":[";
    for(std::size_t i=0;i<run.trace.size();++i) {if(i) out+=',';out+=json_string(run.trace[i]);}
    return out+"]}";
}
std::string pack_fraction_comparison_records(const FractionComparisonRun& run,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records) {
    std::ostringstream payload_stream;
    const auto task=task_text(run.task);
    append_chunked(payload_stream,task,records);
    append_chunked(payload_stream,"RESULT\n",records);
    append_chunked(payload_stream,render_fraction_comparison(run)+"\n",records);
    const auto payload=payload_stream.str();
    const auto bundle=std::string(header(run.task))+sha256(payload,checkpoint)+"\n"+payload;
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    return bundle;
}
std::string pack_fraction_comparison(const FractionComparisonRun& run,const ReplayCheckpoint& checkpoint) {
    return pack_fraction_comparison_records(run,checkpoint,{});
}
FractionComparisonRun replay_fraction_comparison(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    FractionComparisonTask task;
    if(bundle.starts_with("BRUCE_FRACTION_COMPARISON_V2\n")) task.domain_policy="modular";
    if(bundle.starts_with("BRUCE_FRACTION_COMPARISON_V3\n")) task.domain_policy="modular-witness";
    if(!bundle.starts_with(header(task))) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(header(task).size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto payload=bundle.substr(65);
    if(sha256(payload)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    if(checkpoint) checkpoint();
    std::istringstream input{std::string(payload)};
    if(field(input,"engine")!=engine(task) || field(input,"policy")!=policy(task)) throw Error("UnknownRuleOrEngineVersion");
    task.max_bits=integer<unsigned>(field(input,"max_bits"));
    task.max_operations=integer<std::uint64_t>(field(input,"max_operations"));
    const auto cancel=field(input,"cancel");
    if(cancel!="none") task.cancel_after_operations=integer<std::uint64_t>(cancel);
    task.left_numerator=field(input,"left_numerator");task.left_denominator=field(input,"left_denominator");
    task.right_numerator=field(input,"right_numerator");task.right_denominator=field(input,"right_denominator");
    std::string marker;
    if(!std::getline(input,marker) || marker!="RESULT") throw Error("MissingResult");
    if(checkpoint) checkpoint();
    std::function<bool(std::uint64_t)> observe;
    if(checkpoint) observe=[&](std::uint64_t){return detail::observe_replay_checkpoint(checkpoint);};
    FractionComparisonRun run;
    try{run=compare_fractions(std::move(task),observe);}catch(const detail::ReplayCheckpointRaised& stop){std::rethrow_exception(stop.original);}
    if(checkpoint) checkpoint();
    if(task_text(run.task)+"RESULT\n"+render_fraction_comparison(run)+"\n"!=payload)
        throw Error("ReplayMismatch: input, result or logical trace differs");
    return run;
}
}
