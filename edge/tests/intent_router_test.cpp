#include <cassert>

#include "cabinos/intent_router.hpp"

int main() {
    const cabinos::IntentRouter router;

    assert(router.Classify("turn on hazards") == cabinos::Tier::kSafetyCritical);
    assert(router.Classify("set cabin temperature to 22") == cabinos::Tier::kComfort);
    assert(router.Classify("what is battery status") == cabinos::Tier::kComfort);
    assert(router.Classify("find coffee on route") == cabinos::Tier::kCognitive);

    assert(!router.RequiresCloud(cabinos::Tier::kSafetyCritical, true));
    assert(!router.RequiresCloud(cabinos::Tier::kComfort, true));
    assert(router.RequiresCloud(cabinos::Tier::kCognitive, true));
    assert(!router.RequiresCloud(cabinos::Tier::kCognitive, false));

    return 0;
}
