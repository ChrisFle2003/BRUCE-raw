#include "bruce/token/task_spec.hpp"
#include "bruce/evidence/bundle.hpp"
#include <functional>
#include <iostream>

namespace {
void require(bool ok,const char* message) { if(!ok) throw bruce::Error(message); }
bool rejects(const std::function<void()>& action,std::string_view type={}) {
    try { action(); } catch(const bruce::Error& error) { return std::string_view(error.what()).find(type)!=std::string_view::npos; }
    return false;
}
bruce::TokenizedTaskSpecV1 fixture() {
    bruce::TokenizedTaskSpecV1 spec;
    spec.table.version="v1";
    spec.table.entries={{"","Train","",0},{"","Holdout","",1},{"","Rational Portfolio","",2}};
    spec.assets={{"train","x,y\n0,1\n1,1/2\n",bruce::sha256("x,y\n0,1\n1,1/2\n")},
                 {"holdout","x,y\n2,1/3\n",bruce::sha256("x,y\n2,1/3\n")}};
    spec.route_id="rational-portfolio-v1";
    spec.domain="rational";
    spec.budget.candidates=3;
    spec.budget.operations=200;
    spec.budget.trace_events=24;
    spec.budget.results=2;
    return spec;
}
void same_versions_produce_identical_task_spec_bytes() {
    auto first=fixture();
    first=bruce::make_tokenized_task_spec(std::move(first));
    first.roles={{"train",first.table.entries[0].id,"train"},
                 {"holdout",first.table.entries[1].id,"holdout"},
                 {"route",first.table.entries[2].id,""}};
    first.ordered_token_ids={first.table.entries[0].id,first.table.entries[1].id,first.table.entries[2].id};
    first=bruce::make_tokenized_task_spec(std::move(first));
    auto second=first;
    std::swap(second.assets[0],second.assets[1]);
    std::swap(second.roles[0],second.roles[1]);
    require(bruce::pack_tokenized_task_spec(first)==bruce::pack_tokenized_task_spec(second),"same_versions_produce_identical_task_spec_bytes");
    const auto bytes=bruce::pack_tokenized_task_spec(first);
    require(bruce::pack_tokenized_task_spec(bruce::unpack_tokenized_task_spec(bytes))==bytes,"canonical round trip");
    const auto task=bruce::compile_tokenized_task_spec(first);
    require(task.domain=="rational" && task.strategy=="portfolio-rational" && task.planner=="portfolio","wrong route");
    require(task.training.size()==2 && task.holdout.size()==1,"wrong asset role");
}
bruce::TokenizedTaskSpecV1 bound_fixture() {
    auto spec=bruce::make_tokenized_task_spec(fixture());
    spec.ordered_token_ids={spec.table.entries[0].id,spec.table.entries[1].id,spec.table.entries[2].id};
    spec.roles={{"train",spec.table.entries[0].id,"train"},
                {"holdout",spec.table.entries[1].id,"holdout"},
                {"route",spec.table.entries[2].id,""}};
    return bruce::make_tokenized_task_spec(spec);
}
void table_identity_is_version_scoped_and_unsupported_versions_are_rejected() {
    auto spec=bound_fixture();
    require(spec.table.entries[0].id==bruce::sha256("2:v15:train1:0"),"v1 identity lost its version binding");
    require(spec.table.entries[0].id!=bruce::sha256("2:v25:train1:0"),"different table versions share identity");
    for(const auto* version:{"v2","table-1",""}) {
        auto unsupported=fixture(); unsupported.table.version=version;
        require(rejects([&]{(void)bruce::make_tokenized_task_spec(unsupported);},"UnsupportedTokenTableVersion"),"unsupported table accepted by make");
        unsupported=spec; unsupported.table.version=version;
        require(rejects([&]{(void)bruce::pack_tokenized_task_spec(unsupported);},"UnsupportedTokenTableVersion"),"unsupported table accepted by pack");
        require(rejects([&]{(void)bruce::compile_tokenized_task_spec(unsupported);},"UnsupportedTokenTableVersion"),"unsupported table accepted by compile");
    }
}
void v1_rejects_noncanonical_tokens_order_and_role_bindings() {
    auto changed_table=fixture();
    changed_table.table.entries[0].visible_text="Learn";
    require(rejects([&]{(void)bruce::make_tokenized_task_spec(changed_table);},"NoncanonicalTaskSpecV1Table"),
            "noncanonical v1 token table accepted");

    auto reordered=bound_fixture();
    std::swap(reordered.ordered_token_ids[0],reordered.ordered_token_ids[1]);
    require(rejects([&]{(void)bruce::make_tokenized_task_spec(reordered);},"NoncanonicalTaskSpecV1TokenOrder"),
            "reordered v1 token sequence accepted");

    auto rebound=bound_fixture();
    rebound.roles[0].token_id=rebound.table.entries[0].id;
    require(rejects([&]{(void)bruce::make_tokenized_task_spec(rebound);},"NoncanonicalTaskSpecV1RoleBinding"),
            "rebound v1 role token accepted");

    auto swapped_assets=bound_fixture();
    std::swap(swapped_assets.roles[0].asset_alias,swapped_assets.roles[2].asset_alias);
    require(rejects([&]{(void)bruce::make_tokenized_task_spec(swapped_assets);},"NoncanonicalTaskSpecV1RoleAssetBinding"),
            "swapped v1 role assets accepted");
}
void all_native_limit_bounds_are_checked_before_export_and_compile() {
    using Spec=bruce::TokenizedTaskSpecV1;
    struct Limit {const char* name; std::function<void(Spec&,std::uint64_t)> set; std::vector<std::uint64_t> invalid;};
    const std::vector<Limit> limits={
        {"candidates",[](Spec& s,auto n){s.budget.candidates=n;},{0,1000001}},
        {"operations",[](Spec& s,auto n){s.budget.operations=n;},{0,10000001}},
        {"trace_events",[](Spec& s,auto n){s.budget.trace_events=n;},{0,1,100003}},
        {"results",[](Spec& s,auto n){s.budget.results=n;},{0,129}},
        {"max_bits",[](Spec& s,auto n){s.max_bits=n;},{0,16385}},
        {"max_denominator",[](Spec& s,auto n){s.max_denominator=n;},{0,17}},
        {"max_power",[](Spec& s,auto n){s.max_power=n;},{0,9}},
        {"max_degree",[](Spec& s,auto n){s.max_degree=n;},{0,9}},
        {"max_trace_bytes",[](Spec& s,auto n){s.max_trace_bytes=n;},{0,255,4194305}},
    };
    for(const auto& limit:limits) for(const auto value:limit.invalid) {
        auto spec=bound_fixture(); limit.set(spec,value);
        for(const auto& check:std::vector<std::function<void()>>{
                [&]{(void)bruce::make_tokenized_task_spec(spec);},
                [&]{(void)bruce::pack_tokenized_task_spec(spec);},
                [&]{(void)bruce::compile_tokenized_task_spec(spec);}})
            if(!rejects(check)) throw bruce::Error(std::string("invalid limit accepted: ")+limit.name+"="+std::to_string(value));
    }
    for(bool upper:{false,true}) {
        auto spec=bound_fixture();
        spec.assets={{"holdout","x,y\n1,1\n",bruce::sha256("x,y\n1,1\n")},
                     {"train","x,y\n0,0\n",bruce::sha256("x,y\n0,0\n")}};
        spec.minimum="0"; spec.maximum=upper ? "4095" : "0";
        spec.budget={upper ? 1000000U : 1U,upper ? 10000000U : 1U,upper ? 100002U : 2U,upper ? 128U : 1U};
        spec.max_bits=upper ? 16384 : 1; spec.max_denominator=upper ? 16 : 1;
        spec.max_power=upper ? 8 : 1; spec.max_degree=upper ? 8 : 1;
        spec.max_trace_bytes=upper ? 4194304 : 256;
        const auto restored=bruce::unpack_tokenized_task_spec(bruce::pack_tokenized_task_spec(spec));
        (void)bruce::compile_tokenized_task_spec(restored);
    }
}
void exact_range_width_is_materialized() {
    // ASan's use-after-scope instrumentation detects a dangling GMP expression here.
    for(const auto& bounds:std::vector<std::pair<std::string,std::string>>{
            {"0","0"},{"-2048","2047"},{"1000000000000000000000000000000","1000000000000000000000000004095"}}) {
        auto spec=fixture(); spec.minimum=bounds.first; spec.maximum=bounds.second;
        (void)bruce::make_tokenized_task_spec(spec);
    }
    for(const auto& bounds:std::vector<std::pair<std::string,std::string>>{{"1","0"},{"-2048","2048"}}) {
        auto spec=fixture(); spec.minimum=bounds.first; spec.maximum=bounds.second;
        require(rejects([&]{(void)bruce::make_tokenized_task_spec(spec);}),"out-of-range width accepted");
    }
}
void duplicate_alias_is_rejected() {
    auto spec=fixture(); spec.assets[1].alias="train";
    require(rejects([&]{(void)bruce::make_tokenized_task_spec(spec);}),"duplicate_alias_is_rejected");
}
void unselected_ambiguity_cannot_compile() {
    auto spec=bruce::make_tokenized_task_spec(fixture());
    spec.ordered_token_ids={spec.table.entries[0].id,spec.table.entries[1].id,spec.table.entries[2].id};
    spec.alternatives={{"left",{{"train",spec.table.entries[0].id,"train"},{"holdout",spec.table.entries[1].id,"holdout"},{"route",spec.table.entries[2].id,""}}},
                       {"right",{{"train",spec.table.entries[0].id,"train"},{"holdout",spec.table.entries[1].id,"holdout"},{"route",spec.table.entries[2].id,""}}}};
    spec=bruce::make_tokenized_task_spec(std::move(spec));
    require(rejects([&]{(void)bruce::pack_tokenized_task_spec(spec);}),"unselected ambiguity was packed");
    require(rejects([&]{(void)bruce::compile_tokenized_task_spec(spec);}),"unselected_ambiguity_cannot_compile");
    spec.selected_id="left";
    require(bruce::compile_tokenized_task_spec(spec).training.size()==2,"explicit selection failed");
}
void changed_asset_bytes_or_digest_is_rejected() {
    auto spec=bruce::make_tokenized_task_spec(fixture());
    spec.assets[0].bytes.push_back('x');
    require(rejects([&]{(void)bruce::pack_tokenized_task_spec(spec);}),"changed asset bytes accepted");
    spec=bruce::make_tokenized_task_spec(fixture());
    spec.assets[0].digest[0]^=1;
    require(rejects([&]{(void)bruce::compile_tokenized_task_spec(spec);}),"changed asset digest accepted");
}
void compilation_limits_round_trip() {
    auto spec=bruce::make_tokenized_task_spec(fixture());
    spec.ordered_token_ids={spec.table.entries[0].id,spec.table.entries[1].id,spec.table.entries[2].id};
    spec.roles={{"train",spec.table.entries[0].id,"train"},
                {"holdout",spec.table.entries[1].id,"holdout"},
                {"route",spec.table.entries[2].id,""}};
    spec.minimum="-2";
    spec.maximum="3";
    spec.max_power=2;
    spec.max_degree=2;
    const auto bytes=bruce::pack_tokenized_task_spec(spec);
    const auto restored=bruce::unpack_tokenized_task_spec(bytes);
    require(restored.minimum=="-2" && restored.maximum=="3" &&
            restored.max_power==2 && restored.max_degree==2,"compilation limits did not round trip");
    const auto task=bruce::compile_tokenized_task_spec(restored);
    require(task.minimum=="-2" && task.maximum=="3" && task.max_power==2 && task.max_degree==2,
            "compiled task ignored serialized limits");
    require(task.domain=="rational" && task.strategy=="portfolio-rational" && task.planner=="portfolio",
            "fixed route changed");
}
}
int main() {
    unsigned failed=0;
    for(const auto test:{exact_range_width_is_materialized,same_versions_produce_identical_task_spec_bytes,
            table_identity_is_version_scoped_and_unsupported_versions_are_rejected,
            v1_rejects_noncanonical_tokens_order_and_role_bindings,
            all_native_limit_bounds_are_checked_before_export_and_compile,duplicate_alias_is_rejected,
            unselected_ambiguity_cannot_compile,changed_asset_bytes_or_digest_is_rejected,compilation_limits_round_trip}) {
        try { test(); } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; ++failed; }
    }
    return failed ? 1 : 0;
}
