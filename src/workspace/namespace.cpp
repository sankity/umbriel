#include "workspace/namespace.h"

#include <algorithm>

namespace umbriel {

  size_t countInNamespace(const std::vector<std::string_view>& namespaces, std::string_view active) {
    size_t count = 0;
    for (std::string_view entry : namespaces) {
      if (namespaceVisible(entry, active)) {
        ++count;
      }
    }
    return count;
  }

  std::optional<size_t>
  indexInNamespace(const std::vector<std::string_view>& namespaces, std::string_view active, size_t globalIndex) {
    if (globalIndex >= namespaces.size() || !namespaceVisible(namespaces[globalIndex], active)) {
      return std::nullopt;
    }
    size_t position = 0;
    for (size_t index = 0; index < globalIndex; ++index) {
      if (namespaceVisible(namespaces[index], active)) {
        ++position;
      }
    }
    return position;
  }

  std::optional<size_t>
  globalIndexAt(const std::vector<std::string_view>& namespaces, std::string_view active, size_t position) {
    size_t seen = 0;
    for (size_t index = 0; index < namespaces.size(); ++index) {
      if (!namespaceVisible(namespaces[index], active)) {
        continue;
      }
      if (seen == position) {
        return index;
      }
      ++seen;
    }
    return std::nullopt;
  }

  std::optional<size_t>
  nextInNamespace(const std::vector<std::string_view>& namespaces, std::string_view active, size_t globalIndex) {
    if (globalIndex >= namespaces.size()) {
      return std::nullopt;
    }
    for (size_t index = globalIndex + 1; index < namespaces.size(); ++index) {
      if (namespaceVisible(namespaces[index], active)) {
        return index;
      }
    }
    return std::nullopt;
  }

  std::optional<size_t>
  prevInNamespace(const std::vector<std::string_view>& namespaces, std::string_view active, size_t globalIndex) {
    if (globalIndex >= namespaces.size()) {
      return std::nullopt;
    }
    for (size_t index = globalIndex; index-- > 0;) {
      if (namespaceVisible(namespaces[index], active)) {
        return index;
      }
    }
    return std::nullopt;
  }

  std::optional<size_t> clampPositionInNamespace(size_t visibleCount, size_t position, bool dynamic) {
    if (position < visibleCount) {
      return position;
    }
    if (dynamic && visibleCount > 0) {
      return visibleCount - 1;
    }
    return std::nullopt;
  }

  std::vector<size_t>
  prunableIndicesInNamespace(const std::vector<NamespaceInventoryMember>& members, bool emptyAbove, size_t floor) {
    // Keepers mirror WorkspaceGroup::reconcileDynamic restricted to one
    // namespace run: the leading empty (emptyAbove only), the active empty
    // workspace, and one trailing empty (the active doubling as trailing when
    // no substantive member follows it within the run).
    std::optional<size_t> front;
    if (emptyAbove && !members.empty() && !members[0].named && !members[0].occupied) {
      front = 0;
    }
    std::optional<size_t> active;
    for (size_t index = 0; index < members.size(); ++index) {
      if (members[index].isActive && !members[index].named && !members[index].occupied) {
        active = index;
        break;
      }
    }
    std::optional<size_t> back;
    if (active.has_value() && active != front) {
      const bool substantiveFollows = std::any_of(
          members.begin() + static_cast<std::ptrdiff_t>(*active + 1), members.end(),
          [](const NamespaceInventoryMember& member) { return member.named || member.occupied; }
      );
      if (!substantiveFollows) {
        back = active;
      }
    }
    if (!back.has_value() && !members.empty()) {
      const size_t last = members.size() - 1;
      if (!members[last].named && !members[last].occupied && (!front.has_value() || last != *front)) {
        back = last;
      }
    }
    std::vector<size_t> victims;
    size_t surviving = members.size();
    for (size_t index = members.size(); index-- > 0 && surviving > floor;) {
      if (!members[index].named && !members[index].occupied && index != front && index != active && index != back) {
        victims.push_back(index);
        --surviving;
      }
    }
    return victims;
  }

} // namespace umbriel
