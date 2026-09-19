#!/usr/bin/env bash
# Workspace namespaces are per output group: the same namespace string lives
# independently on each output, and switching one output never mutates another.
# harness: outputs=2
set -euo pipefail

accepts() {
  if ! out=$("$UMBRIEL" msg "$1" 2>&1); then
    echo "expected '$1' to be accepted, got: $out"
    return 1
  fi
}

ws() { "$UMBRIEL" workspaces --json; }

active_ns_of() { ws | jq -r --arg output "$1" '[.[] | select(.output == $output)][0].active_namespace'; }
active_of() { ws | jq -r --arg output "$1" '.[] | select(.output == $output and .active) | .id'; }
ns_of() { ws | jq -r --arg id "$1" '.[] | select(.id == $id) | .namespace'; }
count_in_ns() {
  ws | jq -r --arg output "$1" --arg ns "$2" '[.[] | select(.output == $output and .namespace == $ns)] | length'
}

# Switch coding on HEADLESS-1 only.
accepts "namespace-switch:coding/HEADLESS-1"
if [[ $(active_ns_of HEADLESS-1) != "coding" ]]; then
  echo "HEADLESS-1 did not switch to coding: $(ws)"
  exit 1
fi
if [[ $(active_ns_of HEADLESS-2) != "" ]]; then
  echo "HEADLESS-2 leaked the switch: $(ws)"
  exit 1
fi
if [[ $(ns_of "$(active_of HEADLESS-1)") != "coding" ]]; then
  echo "HEADLESS-1 focus is not in coding: $(ws)"
  exit 1
fi

# HEADLESS-2 navigation is unaffected and stays global.
h2_first=$(active_of HEADLESS-2)
accepts "workspace-switch:1/HEADLESS-2"
if [[ $(active_of HEADLESS-2) != "$h2_first" ]]; then
  echo "HEADLESS-2 switch:1 moved unexpectedly: $(ws)"
  exit 1
fi

# The same string is an independent context per output.
accepts "namespace-switch:coding/HEADLESS-2"
if [[ $(active_ns_of HEADLESS-2) != "coding" ]]; then
  echo "HEADLESS-2 did not switch to coding: $(ws)"
  exit 1
fi
h1_count=$(count_in_ns HEADLESS-1 coding)
h2_count=$(count_in_ns HEADLESS-2 coding)
if [[ $h1_count -lt 1 || $h2_count -lt 1 ]]; then
  echo "each output needs its own coding inventory: $(ws)"
  exit 1
fi

# A move on one output does not touch the other output's inventory.
h2_ids_before=$(ws | jq -r '.[] | select(.output == "HEADLESS-2") | .id' | sort)
# The empty coding workspace on HEADLESS-1 is visible there (coding is H1's
# active namespace), so position 1 addresses it. Anonymous numerics are
# positions, never names: quoting would make the lookup miss.
if [[ $(count_in_ns HEADLESS-1 coding) -lt 1 ]]; then
  echo "no coding workspace on HEADLESS-1 to move: $(ws)"
  exit 1
fi
accepts "workspace-set-namespace:1/HEADLESS-1=work"
if [[ $(ws | jq -r '.[] | select(.output == "HEADLESS-2") | .id' | sort) != "$h2_ids_before" ]]; then
  echo "HEADLESS-1 move disturbed HEADLESS-2: $(ws)"
  exit 1
fi
if [[ $(active_ns_of HEADLESS-1) != "coding" ]]; then
  echo "move changed HEADLESS-1 active namespace: $(ws)"
  exit 1
fi

# Unknown output is rejected without touching anything.
before=$(ws)
if out=$("$UMBRIEL" msg "namespace-switch:coding/NOPE-9" 2>&1); then
  echo "expected unknown-output switch to be rejected, but it succeeded"
  exit 1
fi
if [[ $out != *"unknown output"* ]]; then
  echo "expected an unknown-output message, got: $out"
  exit 1
fi
if [[ $(ws) != "$before" ]]; then
  echo "rejected switch changed state: $(ws)"
  exit 1
fi
