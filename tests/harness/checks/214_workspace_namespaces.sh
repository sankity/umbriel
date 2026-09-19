#!/usr/bin/env bash
# Workspace namespaces: switching, assignment, navigation scoping, dynamic
# inheritance, id stability, and identifier validation on one output.
set -euo pipefail

readonly BASELINE="$(< "$UMBRIEL_CONFIG")"
readonly CLIENT="${UMBRIEL_UNMAP_CLIENT:-./build-debug/tests/unmap-client}"

if [[ ! -x $CLIENT ]]; then
  echo "unmap client not built at $CLIENT"
  exit 1
fi

accepts() {
  if ! out=$("$UMBRIEL" msg "$1" 2>&1); then
    echo "expected '$1' to be accepted, got: $out"
    return 1
  fi
}

rejects_with() {
  local action=$1 expected=$2
  if out=$("$UMBRIEL" msg "$action" 2>&1); then
    echo "expected '$action' to be rejected, but it succeeded"
    return 1
  fi
  if [[ $out != *"$expected"* ]]; then
    echo "expected '$action' to mention '$expected', got: $out"
    return 1
  fi
}

ws() { "$UMBRIEL" workspaces --json; }

focused_id() { ws | jq -r '.[] | select(.focused) | .id'; }
focused_ns() { ws | jq -r '.[] | select(.focused) | .namespace'; }
active_ns_all() { ws | jq -r '[.[].active_namespace] | unique | join(",")'; }
ids_sorted() { ws | jq -r '.[].id' | sort; }
ns_of() { ws | jq -r --arg id "$1" '.[] | select(.id == $id) | .namespace'; }
first_in_ns() { ws | jq -r --arg ns "$1" '[.[] | select(.namespace == $ns)][0].id'; }
count_in_ns() { ws | jq -r --arg ns "$1" '[.[] | select(.namespace == $ns)] | length'; }
name_of() { ws | jq -r --arg id "$1" '.[] | select(.id == $id) | .name'; }

# Move a workspace by stable id. Named workspaces resolve globally (quoted or
# bare); anonymous workspaces are positions, so resolve the id to its position
# inside its own visible namespace first.
move_named() {
  accepts "workspace-set-namespace:$1=$2"
}

move_by_id() {
  local id=$1 ns=$2 name pos here there
  name=$(name_of "$id")
  if [[ -z $name ]]; then
    echo "no workspace with id $id: $(ws)"
    return 1
  fi
  if [[ $name =~ ^[0-9]+$ ]]; then
    # Anonymous: only addressable by position while its own namespace is active.
    here=$(active_ns_all)
    there=$(ns_of "$id")
    if [[ $there != "$here" ]]; then
      echo "cannot address hidden anonymous workspace $id ($there) from $here"
      return 1
    fi
    pos=$(ws | jq -r --arg id "$id" --arg ns "$there" '
      ([.[] | select(.namespace == $ns)] | map(.id) | index($id)) + 1')
    accepts "workspace-set-namespace:${pos}=${ns}"
  else
    accepts "workspace-set-namespace:${name}=${ns}"
  fi
}

spawn_client() {
  APP_ID="$1" "$CLIENT" "$1" 800 600 > "$UMBRIEL_RUNTIME_DIR/$1.log" 2>&1 &
}

wait_for_windows() {
  local expected=$1 count=
  for _ in $(seq 40); do
    count=$("$UMBRIEL" windows --json | jq 'length')
    [[ $count == "$expected" ]] && return 0
    sleep 0.1
  done
  echo "expected $expected window(s), got $count"
  return 1
}

window_workspace() {
  "$UMBRIEL" windows --json | jq -r --arg title "$1" '.[] | select(.title == $title) | .workspace'
}

# Default namespace behaves as before.
snapshot=$(ws)
if ! jq -e 'all(.[]; .namespace == "" and .active_namespace == "")' <<< "$snapshot" > /dev/null; then
  echo "expected a clean default namespace start, got: $snapshot"
  exit 1
fi

# A named workspace gives hidden-namespace moves something addressable:
# anonymous numerics are positions, not names, once another namespace hides them.
printf '%s\n\n%s\n' "$BASELINE" '[output.HEADLESS-1]
workspaces = "dynamic"

[[workspace]]
name = "CHAT"' > "$UMBRIEL_CONFIG"
"$UMBRIEL" msg config-reload > /dev/null
chat_id=$(ws | jq -r '.[] | select(.name == "CHAT") | .id')
if [[ -z $chat_id ]]; then
  echo "CHAT workspace did not materialize: $(ws)"
  exit 1
fi

# Switch default -> coding.
accepts "namespace-switch:coding"
if [[ $(active_ns_all) != "coding" ]]; then
  echo "active namespace did not become coding: $(ws)"
  exit 1
fi
if [[ $(focused_ns) != "coding" ]]; then
  echo "focused workspace is not in coding after switch: $(ws)"
  exit 1
fi
coding_first=$(first_in_ns coding)
if [[ $(focused_id) != "$coding_first" ]]; then
  echo "switch did not land on the first coding workspace: $(ws)"
  exit 1
fi
# Existing ids survive the switch.
if ! ids_sorted | grep -qxF "$chat_id"; then
  echo "CHAT id did not survive the switch: $(ws)"
  exit 1
fi

# Switching to the same namespace is idempotent.
before=$(ws)
accepts "namespace-switch:coding"
if [[ $(ws) != "$before" ]]; then
  echo "idempotent switch changed state: $(ws)"
  exit 1
fi

# Numeric selectors resolve inside the active namespace.
accepts "workspace-switch:1"
if [[ $(focused_id) != "$coding_first" ]]; then
  echo "workspace-switch:1 left the coding context: $(ws)"
  exit 1
fi
accepts "workspace-next"
if [[ $(focused_id) != "$coding_first" ]]; then
  echo "workspace-next escaped a single-workspace namespace: $(ws)"
  exit 1
fi
accepts "workspace-previous"
if [[ $(focused_id) != "$coding_first" ]]; then
  echo "workspace-previous escaped a single-workspace namespace: $(ws)"
  exit 1
fi

# A dynamic workspace is created in the active namespace.
spawn_client ns-main
wait_for_windows 1
main_ws=$(window_workspace ns-main)
if [[ $(ns_of "$main_ws") != "coding" ]]; then
  echo "spawned window did not land in coding: $(ws) / $("$UMBRIEL" windows --json)"
  exit 1
fi
if [[ $(ws | jq -r '[.[] | select(.namespace == "coding" and .occupied == false)] | length') -lt 1 ]]; then
  echo "no coding sentinel after spawn: $(ws)"
  exit 1
fi
occupied_coding=$main_ws

# Move the named workspace (hidden in "") into work.
move_by_id "$chat_id" work
if [[ $(ns_of "$chat_id") != "work" ]]; then
  echo "CHAT did not move to work: $(ws)"
  exit 1
fi
# It is visible only there: addressing it from coding is rejected.
rejects_with "workspace-switch:CHAT" "not in the active namespace"
# Switch into work and confirm the move.
accepts "namespace-switch:work"
if [[ $(focused_id) != "$chat_id" ]]; then
  echo "work switch did not land on CHAT: $(ws)"
  exit 1
fi
# Back to coding: the compositor keeps a valid visible selection.
accepts "namespace-switch:coding"
if [[ $(focused_ns) != "coding" ]]; then
  echo "return to coding stranded focus: $(ws)"
  exit 1
fi

# Move the occupied coding workspace into work; its window follows by id.
move_by_id "$occupied_coding" work
if [[ $(ns_of "$occupied_coding") != "work" ]]; then
  echo "occupied workspace did not move: $(ws)"
  exit 1
fi
if [[ $(window_workspace ns-main) != "$occupied_coding" ]]; then
  echo "window did not follow its workspace id: $("$UMBRIEL" windows --json)"
  exit 1
fi
if [[ $(focused_ns) != "coding" ]]; then
  echo "moving the active workspace stranded focus: $(ws)"
  exit 1
fi

# Move an empty dynamic coding workspace into work.
empty_coding=$(ws | jq -r '[.[] | select(.namespace == "coding" and .occupied == false)][0].id')
if [[ -z $empty_coding ]]; then
  echo "no empty coding workspace to move: $(ws)"
  exit 1
fi
move_by_id "$empty_coding" work
if [[ $(ns_of "$empty_coding") != "work" ]]; then
  echo "empty workspace did not move: $(ws)"
  exit 1
fi

# Walk next across coding; every stop stays inside the namespace.
seen=0
prev=""
for _ in $(seq 10); do
  current=$(focused_id)
  if [[ $current == "$prev" ]]; then
    break
  fi
  prev=$current
  if [[ $(ns_of "$current") != "coding" ]]; then
    echo "next-chain escaped coding at $current: $(ws)"
    exit 1
  fi
  seen=$((seen + 1))
  accepts "workspace-next"
done
if [[ $seen -ne $(count_in_ns coding) ]]; then
  echo "next-chain visited $seen of $(count_in_ns coding) coding workspaces: $(ws)"
  exit 1
fi

# Move back to default and confirm global behavior returns.
accepts "namespace-switch"
if [[ $(active_ns_all) != "" ]]; then
  echo "bare switch did not restore default: $(ws)"
  exit 1
fi
accepts "workspace-switch:CHAT"
if [[ $(focused_id) != "$chat_id" ]]; then
  echo "default namespace cannot reach CHAT by name: $(ws)"
  exit 1
fi

# Tracked ids are unchanged end to end.
for id in "$chat_id" "$occupied_coding"; do
  if ! ids_sorted | grep -qxF "$id"; then
    echo "workspace id $id changed or vanished: $(ws)"
    exit 1
  fi
done

# Invalid identifiers and shapes.
rejects_with "namespace-switch:a=b" "unknown action"
rejects_with "namespace-switch:coding/" "unknown action"
rejects_with "namespace-switch:a/b" "unknown output"
rejects_with "workspace-set-namespace:1" "unknown action"
rejects_with "workspace-set-namespace:1=a/b" "unknown action"
rejects_with "workspace-set-namespace:DOESNOTEXIST=nope" "unknown workspace"
