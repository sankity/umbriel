#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace umbriel {

  // Workspace namespace foundation (Phase 1): workspaces carry an opaque
  // namespace id, groups select one active namespace. Empty means the default
  // global namespace and preserves existing behavior: every workspace is
  // visible. These helpers are the single source of truth for namespace-relative
  // positions, so WorkspaceGroup delegates to them instead of reimplementing
  // the math. They stay free of wlroots/Server/Output so unit tests cover them
  // without a running compositor.
  [[nodiscard]] constexpr bool namespaceVisible(std::string_view workspaceNamespace, std::string_view activeNamespace) {
    return activeNamespace.empty() || workspaceNamespace == activeNamespace;
  }

  // Number of entries in `namespaces` visible under `active`.
  [[nodiscard]] size_t countInNamespace(const std::vector<std::string_view>& namespaces, std::string_view active);

  // Display position (0-based rank among visible entries) of the entry at
  // `globalIndex`. Nullopt when out of range or hidden by `active`.
  [[nodiscard]] std::optional<size_t>
  indexInNamespace(const std::vector<std::string_view>& namespaces, std::string_view active, size_t globalIndex);

  // Global index of the entry at display position `position`. Nullopt when out
  // of range.
  [[nodiscard]] std::optional<size_t>
  globalIndexAt(const std::vector<std::string_view>& namespaces, std::string_view active, size_t position);

  // Dynamic-inventory clamp policy for a requested display position: in-range
  // positions pass through, out-of-range positions clamp to the last visible
  // entry on dynamic groups (mirroring WorkspaceGroup::workspaceAtClamped) and
  // miss on static groups or when nothing is visible.

  // Nearest visible global index strictly after/before `globalIndex`. Nullopt
  // at the edges or when `globalIndex` is out of range.
  [[nodiscard]] std::optional<size_t>
  nextInNamespace(const std::vector<std::string_view>& namespaces, std::string_view active, size_t globalIndex);
  [[nodiscard]] std::optional<size_t>
  prevInNamespace(const std::vector<std::string_view>& namespaces, std::string_view active, size_t globalIndex);
  [[nodiscard]] std::optional<size_t> clampPositionInNamespace(size_t visibleCount, size_t position, bool dynamic);

  // One member of a single-namespace run for dynamic prune decisions.
  struct NamespaceInventoryMember {
    bool named = false;
    bool occupied = false;
    bool isActive = false;
  };
  // Run indices (into the caller's member list, in inventory order) that
  // dynamic reconciliation may prune: anonymous, empty, non-keeper members,
  // collected from the end while the run stays above `floor`. Named,
  // occupied, keeper, and floor-protected members are never listed. Returned
  // in descending order so the caller can erase front-to-back without
  // disturbing pending indices. Pure so unit tests pin the policy without a
  // compositor; WorkspaceGroup translates run indices to vector slots.
  [[nodiscard]] std::vector<size_t>
  prunableIndicesInNamespace(const std::vector<NamespaceInventoryMember>& members, bool emptyAbove, size_t floor);

} // namespace umbriel
