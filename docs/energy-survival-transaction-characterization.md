# Energy / Survival Transaction Contract

This document records the current fixed point for energy/survival behavior on `agent/ownership-contracts-settings`. It is descriptive: it preserves proven behavior and separates authoritative state from derived feedback. It does not authorize gameplay-value changes.

## Authoritative transaction

`Game::spendBattery()` currently owns the state transition:

```text
BatteryRequest(amount, reason)
→ hit-only mitigation
→ stored-soul drain multiplier
→ supplemental battery absorption
→ main battery subtraction
→ zero-battery survival resolution
   → eligible hit: last stand
   → multiplayer hit: downed
   → solo hit + stored soul + unused allowance: soul reboot
   → otherwise: run death
```

Focused assertions in `native/tests/gameplay_state_contracts_test.cpp` exercise these branches through normal simulation paths rather than by exposing `spendBattery()` directly.

## Proven precedence

### Hit mitigation precedes resource loss

For `BatteryReason::Hit`:

- survival synergy scales incoming cost first;
- active impact guard further scales the hit;
- only the resulting amount reaches the resource layers.

The focused human-hit fixture proves both tier-1 survival mitigation and impact-guard mitigation.

### Stored-soul efficiency precedes supplemental absorption

A real 26-point human hit with one stored soul and 10 supplemental power proves this ordering:

```text
26
→ divide by 1.16 stored-soul efficiency
→ consume 10 supplemental
→ subtract only the remainder from main battery
```

This is an authoritative amount contract, not merely a source observation.

### Supplemental power is first-loss energy

After scaling, supplemental power absorbs cost before `PlayerState::battery` changes. Exhausting it clears the active supplemental-power state.

### Last stand has first claim on an eligible zero-battery hit

At zero battery, an eligible hit with survival synergy and a ready cooldown becomes:

- battery `1.0`;
- last-stand cooldown set;
- no downed/reboot/death result.

The test reaches this through the real human-swing path at the `22.0` battery damage/grab boundary.

### Multiplayer hit exhaustion becomes downed

If last stand does not claim the result and a multiplayer hit reaches zero:

- player becomes downed;
- bleedout timer becomes 15 seconds;
- revive charge and movement/lifecycle action state reset;
- immediate run death does not occur.

### Solo hit exhaustion may consume one stored soul

If last stand does not claim the result, multiplayer is disabled, a soul is stored, and the room-scoped reboot has not been used:

- one soul is consumed;
- battery becomes 15;
- `soloSoulRebootUsed` becomes true;
- passive recharge is briefly locked;
- downed/death do not occur.

`advanceClearedRoom()` resets the reboot allowance, so this fallback is room-scoped.

### Non-hit exhaustion can end the run

A real ground-jump request from insufficient battery proves that ordinary non-hit exhaustion bypasses the hit-only survival branches and triggers run death.

## Feedback ownership

HUD/audio publication is not authoritative energy state.

`setEnergyTicker()` writes transient HUD presentation. End-of-frame HUD battery fill/low-battery/game-over values are derived from authoritative state. `updateBatteryAudio()` observes battery thresholds and publishes audio events/arming state; those values do not decide battery or survival outcomes.

The browser reference uses the same conceptual split: battery mutation calls an event-sound observer for low/connect-power cues, while gameplay outcomes remain owned by battery state.

Therefore the stable contract is:

```text
authoritative EnergyOutcome
→ derived ticker / HUD / audio publication
```

Preserve which cue/message corresponds to a semantic result. Do **not** freeze the incidental line-by-line order of ticker mutation versus audio publication inside `spendBattery()` as gameplay authority.

## Current evidence

Focused gameplay coverage proves:

- supplemental-first loss;
- stored-soul efficiency before supplemental absorption;
- survival-synergy mitigation;
- impact-guard mitigation;
- last-stand precedence;
- multiplayer downed precedence;
- solo soul-reboot precedence;
- non-hit exhaustion death.

Key evidence:

- `native/tests/gameplay_state_contracts_test.cpp`
- Gameplay checks `32171862921` — initial seven survival cases passed;
- Gameplay checks `32173971562` / subsequent exact-head runs — stored-soul ordering passed.

## Extraction rule

A future `EnergyResult` is allowed to separate authoritative mutation from feedback publication, but it must preserve the proven state precedence and constants. Useful result facts may include:

- scaled requested cost;
- supplemental absorbed amount;
- main battery before/after;
- last stand triggered;
- player downed;
- soul reboot triggered;
- run death triggered;
- semantic feedback category.

Do not restructure this transaction merely because the tests now make it possible. Extraction should happen only when it simplifies an actual ownership/caller problem.
