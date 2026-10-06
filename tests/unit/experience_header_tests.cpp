#include "bruce/experience/store.hpp"
namespace bruce {struct ExactResearchTask;struct PortfolioOrder;}
template<class T> concept Complete = requires { sizeof(T); };
static_assert(!Complete<bruce::ExactResearchTask>);
static_assert(!Complete<bruce::PortfolioOrder>);
int main() { bruce::ExperienceStore store; return store.records.empty() ? 0 : 1; }
