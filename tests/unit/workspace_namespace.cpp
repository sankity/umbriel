#include "check.h"
#include "workspace/namespace.h"

#include <string_view>
#include <vector>

using umbriel::clampPositionInNamespace;
using umbriel::countInNamespace;
using umbriel::globalIndexAt;
using umbriel::indexInNamespace;
using umbriel::NamespaceInventoryMember;
using umbriel::namespaceVisible;
using umbriel::nextInNamespace;
using umbriel::prevInNamespace;
using umbriel::prunableIndicesInNamespace;

namespace {

  std::vector<std::string_view> makeNamespaces(std::initializer_list<std::string_view> entries) {
    return std::vector<std::string_view>(entries);
  }

  umbriel::NamespaceInventoryMember member(bool named, bool occupied, bool isActive = false) {
    return {.named = named, .occupied = occupied, .isActive = isActive};
  }

  // Shorthands: anonymous/empty, anonymous/occupied, named/empty.
  umbriel::NamespaceInventoryMember empty(bool isActive = false) { return member(false, false, isActive); }
  umbriel::NamespaceInventoryMember occupied() { return member(false, true); }
  umbriel::NamespaceInventoryMember named() { return member(true, false); }

} // namespace

UMBRIEL_TEST(defaultNamespaceExposesEveryWorkspace) {
  const auto entries = makeNamespaces({"", "", ""});
  CHECK(namespaceVisible("", ""));
  CHECK_EQ(countInNamespace(entries, ""), 3UZ);
  CHECK_EQ(indexInNamespace(entries, "", 0).value_or(99), 0UZ);
  CHECK_EQ(indexInNamespace(entries, "", 2).value_or(99), 2UZ);
  CHECK_EQ(globalIndexAt(entries, "", 1).value_or(99), 1UZ);
}

UMBRIEL_TEST(activeNamespaceFiltersToMembers) {
  const auto entries = makeNamespaces({"coding", "work", "coding"});
  CHECK_EQ(countInNamespace(entries, "coding"), 2UZ);
  CHECK_EQ(countInNamespace(entries, "work"), 1UZ);
  CHECK_EQ(countInNamespace(entries, "school"), 0UZ);
}

UMBRIEL_TEST(displayIndexIsRankWithinNamespace) {
  const auto entries = makeNamespaces({"coding", "work", "coding", "coding"});
  CHECK_EQ(indexInNamespace(entries, "coding", 0).value_or(99), 0UZ);
  CHECK_EQ(indexInNamespace(entries, "coding", 2).value_or(99), 1UZ);
  CHECK_EQ(indexInNamespace(entries, "coding", 3).value_or(99), 2UZ);
  CHECK_EQ(indexInNamespace(entries, "work", 1).value_or(99), 0UZ);
}

UMBRIEL_TEST(hiddenWorkspaceHasNoDisplayIndex) {
  const auto entries = makeNamespaces({"coding", "work"});
  CHECK(!indexInNamespace(entries, "coding", 1).has_value());
  CHECK(!indexInNamespace(entries, "work", 0).has_value());
}

UMBRIEL_TEST(outOfRangeGlobalIndexHasNoDisplayIndex) {
  const auto entries = makeNamespaces({"coding"});
  CHECK(!indexInNamespace(entries, "", 1).has_value());
  CHECK(!indexInNamespace(entries, "coding", 7).has_value());
}

UMBRIEL_TEST(globalIndexAtRoundTripsWithDisplayIndex) {
  const auto entries = makeNamespaces({"coding", "work", "coding"});
  CHECK_EQ(globalIndexAt(entries, "coding", 0).value_or(99), 0UZ);
  CHECK_EQ(globalIndexAt(entries, "coding", 1).value_or(99), 2UZ);
  CHECK(!globalIndexAt(entries, "coding", 2).has_value());
  CHECK(!globalIndexAt(entries, "school", 0).has_value());
}

UMBRIEL_TEST(neighborsSkipOtherNamespaces) {
  const auto entries = makeNamespaces({"coding", "work", "coding", "work", "coding"});
  CHECK_EQ(nextInNamespace(entries, "coding", 0).value_or(99), 2UZ);
  CHECK_EQ(nextInNamespace(entries, "coding", 2).value_or(99), 4UZ);
  CHECK_EQ(prevInNamespace(entries, "coding", 4).value_or(99), 2UZ);
  CHECK_EQ(prevInNamespace(entries, "coding", 2).value_or(99), 0UZ);
  CHECK_EQ(nextInNamespace(entries, "work", 1).value_or(99), 3UZ);
}

UMBRIEL_TEST(neighborsAtEdgesAreEmpty) {
  const auto entries = makeNamespaces({"coding", "work", "coding"});
  CHECK(!nextInNamespace(entries, "coding", 2).has_value());
  CHECK(!prevInNamespace(entries, "coding", 0).has_value());
  CHECK(!nextInNamespace(entries, "", 2).has_value());
  CHECK(!prevInNamespace(entries, "", 0).has_value());
}

UMBRIEL_TEST(outOfRangeNeighborsAreEmpty) {
  const auto entries = makeNamespaces({"coding"});
  CHECK(!nextInNamespace(entries, "", 5).has_value());
  CHECK(!prevInNamespace(entries, "coding", 5).has_value());
}

UMBRIEL_TEST(emptyWorkspaceNamespaceNeverMatchesNonEmptyActive) {
  const auto entries = makeNamespaces({"", "coding"});
  CHECK(!namespaceVisible("", "coding"));
  CHECK_EQ(countInNamespace(entries, "coding"), 1UZ);
  CHECK(!indexInNamespace(entries, "coding", 0).has_value());
}

UMBRIEL_TEST(switchPositionsResolveInsideActiveNamespace) {
  // workspace-switch:1/:2 with coding active: first/second coding workspace,
  // never the interleaved work one.
  const auto entries = makeNamespaces({"coding", "work", "coding"});
  CHECK_EQ(globalIndexAt(entries, "coding", 0).value_or(99), 0UZ);
  CHECK_EQ(globalIndexAt(entries, "coding", 1).value_or(99), 2UZ);
  CHECK(!globalIndexAt(entries, "coding", 2).has_value());
}

UMBRIEL_TEST(singleWorkspaceNamespaceHasNoNeighbors) {
  const auto entries = makeNamespaces({"work", "coding", "work"});
  CHECK_EQ(countInNamespace(entries, "coding"), 1UZ);
  CHECK(!nextInNamespace(entries, "coding", 1).has_value());
  CHECK(!prevInNamespace(entries, "coding", 1).has_value());
}

UMBRIEL_TEST(emptyActiveNamespaceKeepsGlobalNeighbors) {
  const auto entries = makeNamespaces({"coding", "work"});
  CHECK_EQ(nextInNamespace(entries, "", 0).value_or(99), 1UZ);
  CHECK_EQ(prevInNamespace(entries, "", 1).value_or(99), 0UZ);
}

UMBRIEL_TEST(neighborScanFromHiddenIndexFindsVisibleMembers) {
  // Pure helpers scan by global index; the WorkspaceGroup wrappers additionally
  // reject hidden sources, so navigation can never start outside the context.
  const auto entries = makeNamespaces({"coding", "work"});
  CHECK_EQ(prevInNamespace(entries, "coding", 1).value_or(99), 0UZ);
  CHECK(!nextInNamespace(entries, "coding", 1).has_value());
}

UMBRIEL_TEST(clampPositionPassesThroughInRange) {
  CHECK_EQ(clampPositionInNamespace(3, 0, false).value_or(99), 0UZ);
  CHECK_EQ(clampPositionInNamespace(3, 2, false).value_or(99), 2UZ);
  CHECK_EQ(clampPositionInNamespace(3, 1, true).value_or(99), 1UZ);
}

UMBRIEL_TEST(clampPositionClampsToLastOnDynamic) {
  // workspace-switch:N beyond the namespace end selects the last member,
  // mirroring workspaceAtClamped.
  CHECK_EQ(clampPositionInNamespace(2, 2, true).value_or(99), 1UZ);
  CHECK_EQ(clampPositionInNamespace(2, 64, true).value_or(99), 1UZ);
}

UMBRIEL_TEST(clampPositionMissesOnStatic) {
  CHECK(!clampPositionInNamespace(2, 2, false).has_value());
  CHECK(!clampPositionInNamespace(2, 64, false).has_value());
}

UMBRIEL_TEST(clampPositionMissesWhenNothingVisible) {
  CHECK(!clampPositionInNamespace(0, 0, false).has_value());
  CHECK(!clampPositionInNamespace(0, 0, true).has_value());
}

UMBRIEL_TEST(pruneRemovesNonKeeperEmptiesAboveFloor) {
  // [occupied, empty, empty(empty trailing)]: the middle empty goes, the
  // trailing sentinel stays.
  const std::vector<umbriel::NamespaceInventoryMember> run = {occupied(), empty(), empty()};
  CHECK_EQ(prunableIndicesInNamespace(run, false, 1UZ), std::vector<size_t>{1});
}

UMBRIEL_TEST(pruneKeepsActiveEmptyWorkspace) {
  const std::vector<umbriel::NamespaceInventoryMember> run = {occupied(), empty(true)};
  CHECK(prunableIndicesInNamespace(run, false, 1).empty());
}

UMBRIEL_TEST(pruneNeverTouchesNamedOrOccupied) {
  const std::vector<umbriel::NamespaceInventoryMember> run = {named(), occupied(), empty()};
  // Nothing prunable above the floor: the trailing empty is a keeper.
  CHECK(prunableIndicesInNamespace(run, false, 3).empty());
  // A named member never becomes a victim even when pruning runs.
  const std::vector<umbriel::NamespaceInventoryMember> roomy = {named(), empty(), empty()};
  CHECK_EQ(prunableIndicesInNamespace(roomy, false, 1UZ), std::vector<size_t>{1});
}

UMBRIEL_TEST(pruneFloorProtectsEmpties) {
  const std::vector<umbriel::NamespaceInventoryMember> run = {empty(), empty()};
  CHECK(prunableIndicesInNamespace(run, false, 2).empty());
  // Trailing sentinel kept; the other empty is pruned.
  CHECK_EQ(prunableIndicesInNamespace(run, false, 1UZ), std::vector<size_t>{0});
}

UMBRIEL_TEST(pruneProtectsLeadingSentinelWhenEnabled) {
  const std::vector<umbriel::NamespaceInventoryMember> run = {empty(), occupied()};
  CHECK(prunableIndicesInNamespace(run, true, 1).empty());
  // Without emptyAbove there is no leading keeper: the leading empty goes.
  CHECK_EQ(prunableIndicesInNamespace(run, false, 1UZ), std::vector<size_t>{0});
}

UMBRIEL_TEST(pruneActiveDoublesAsTrailingSentinel) {
  // [occupied, active-empty, empty]: nothing substantive follows the active
  // workspace, so it serves as the trailing sentinel; the last empty goes.
  const std::vector<umbriel::NamespaceInventoryMember> run = {occupied(), empty(true), empty()};
  CHECK_EQ(prunableIndicesInNamespace(run, false, 1UZ), std::vector<size_t>{2});
}

UMBRIEL_TEST(pruneActiveWithSubstantiveFollowerKeepsTrailing) {
  // [active-empty, occupied, empty]: the active workspace cannot serve as the
  // trailing sentinel while an occupied member follows it.
  const std::vector<umbriel::NamespaceInventoryMember> run = {empty(true), occupied(), empty()};
  CHECK(prunableIndicesInNamespace(run, false, 1).empty());
}

UMBRIEL_TEST(pruneVictimsReturnDescending) {
  const std::vector<umbriel::NamespaceInventoryMember> run = {empty(), empty(), empty(), occupied()};
  // Front keeper (emptyAbove) and floor 1: prune from the end first.
  CHECK_EQ(prunableIndicesInNamespace(run, true, 1UZ), (std::vector<size_t>{2, 1}));
}

int main() { return RUN_TESTS(); }
