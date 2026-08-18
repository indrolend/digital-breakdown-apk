# Energy / Survival Transaction Characterization

Base originally observed: `agent/ownership-contracts-settings` after `4af4a613578ab6f82d571d86e34f54baef16f489`.

Current evidence: core survival/resource precedence is now regression-tested through real gameplay paths in `native/tests/gameplay_state_contracts_test.cpp`. Exact characterization head `ba5ef85539a0146935e933103989533f7848bef0` passed Gameplay checks run `32171862921`, including configure, build, CTest, parity, multiplayer, determinism, peer isolation, input-edge, mobile-framing, and diff checks.

This document remains descriptive. It is not permission to change gameplay values or precedence.

## Why this exists

`Game::spendBattery()` is not a numeric subtraction helper. It is the current authoritative transaction boundary for battery cost, supplemental absorption, hit mitigation, survival fallbacks, HUD feedback, battery audio, multiplayer downed state, solo soul reboot, and run death.

Any extraction must preserve branch order unless a gameplay change explicitly intends otherwise.

## Current transaction order

For an alive player:

```text
BatteryRequest(amount, reason)
→ hit-only mitigation
→ soul inventory drain multiplier
→ supplemental battery absorption
→ main battery subtraction
→ non-continuous HUD ticker
→ battery threshold audio
→ hit-only last stand check
→ zero-battery survival resolution
   → multiplayer hit: downed
   → solo hit + soul + unused reboot: soul reboot
   → otherwise: run death
→ success/failure result
```

## 1. Hit mitigation happens before resource absorption — PROVEN

For `BatteryReason::Hit` only:

- survival synergy reduces incoming amount;
- active impact guard further scales the hit;
- the mitigated amount is what proceeds into the normal battery-cost path.

Focused gameplay assertions now exercise a real human swing and prove both the tier-1 survival scale and impact-guard scale before resource loss.

This means impact guard/survival are damage-policy modifiers, not post-damage refunds.

## 2. Stored souls alter ordinary battery drain before supplemental absorption — OBSERVED

The cost is multiplied by `batteryDrainMultiplier()`, which depends on stored soul count, before supplemental power is consumed.

Current interpretation:

```text
requested amount
→ reason-specific mitigation
→ stored-soul efficiency
→ supplemental battery
→ main battery
```

The solo-reboot test exercises a hit while one soul is stored, but it does not independently isolate the exact scaled amount before supplemental absorption. Do not swap supplemental absorption and the soul-efficiency multiplier without treating that as a gameplay change.

## 3. Supplemental battery is first-loss energy — PROVEN

`consumeSupplementalBattery()` absorbs as much of the scaled cost as possible before main battery changes.

The focused gameplay contract starts with 100 main battery and 10 supplemental power, receives an authoritative 26-point human hit, and verifies that supplemental power is exhausted while main battery loses only the remaining amount.

If supplemental value is exhausted, `clearActivePowerups()` clears the active supplemental-power state.

This is direct evidence that `EnergyState` and `PlayerState::battery` are separate resource layers, not interchangeable fields.

## 4. Feedback currently occurs before zero-battery survival fallback — OBSERVED

After main battery subtraction:

- non-continuous requests publish an energy ticker;
- battery threshold audio is evaluated;
- only then does the transaction evaluate last stand / downed / soul reboot / death.

The survival outcome branches are now regression-tested, but the exact intermediate ticker/audio ordering is not independently asserted. A future authoritative transaction object may separate mutation from feedback publication only after this observable ordering is either characterized or deliberately declared non-contractual.

## 5. Last stand has first claim on a zero-battery hit — PROVEN

If all are true:

- reason is `Hit`;
- battery reached zero;
- survival synergy tier is positive;
- last-stand cooldown is ready;

then:

- battery becomes `1.0`;
- last-stand cooldown is set;
- ticker becomes `LAST SIGNAL`;
- transaction succeeds;
- downed/reboot/death resolution does not run.

The focused test uses the real human-hit path at exactly `22.0` battery: this is the damage/grab boundary where the swing still resolves as a hit, while tier-1 survival mitigation still leaves enough damage to reach zero. The test proves last stand claims the result before other zero-battery survival branches.

## 6. Multiplayer hit zero resolves to downed before death — PROVEN

If battery remains zero after last-stand evaluation and reason is `Hit` while multiplayer is enabled:

- player becomes downed;
- bleedout timer becomes 15 seconds;
- revive charge resets;
- velocity/jump velocity clear;
- player lifecycle actions clear;
- ticker becomes `SIGNAL DOWN`;
- transaction returns failure;
- immediate run death does not occur.

The focused test configures a real host session and drives an authoritative human swing through the same `22.0` battery boundary. It verifies zero battery enters downed state without triggering run death.

Bleedout resolution remains handled later by `updateBattery()`.

## 7. Solo hit zero may consume one stored soul for reboot — PROVEN

If battery remains zero after last-stand evaluation and:

- reason is `Hit`;
- multiplayer is disabled;
- player has at least one stored soul;
- solo soul reboot has not already been used;

then:

- one soul is consumed;
- battery becomes 15;
- solo reboot is marked used;
- passive recharge is locked briefly;
- ticker becomes `SOUL REBOOT`;
- transaction succeeds;
- run death does not occur.

The focused test reaches this branch through an authoritative human swing, verifies exactly one soul is consumed, battery returns to 15, the room-scoped reboot flag is set, and neither downed nor death wins the transaction.

The room-advance policy explicitly resets `soloSoulRebootUsed`, so this fallback remains room-scoped.

## 8. Remaining zero-battery cases trigger run death — PROVEN for non-hit action exhaustion

If no prior survival branch claims the zero-battery state, `triggerRunDeath()` owns final run-death mutation and presentation setup.

A focused test now drives ordinary ground-jump battery spend from insufficient battery and verifies non-hit exhaustion kills the run rather than invoking hit-only survival fallbacks.

## Ownership target

The eventual shape should preserve the proven precedence while making authority explicit:

```text
EnergyRequest
+ Energy/Player/Progression/Network context
→ EnergyResult
→ authoritative state mutation
→ feedback publication
```

Possible `EnergyResult` facts include:

- supplemental absorbed amount;
- main battery spent;
- battery before/after;
- last stand triggered;
- player downed;
- soul reboot triggered;
- player/run died;
- requested feedback category.

Focused survival coverage now exists, so a future extraction is no longer blocked by the main survival branches. That does **not** mean extraction is automatically the next action; the remaining feedback-order contract and transaction shape should be decided first.

## Focused test evidence

`native/tests/gameplay_state_contracts_test.cpp` now proves through normal simulation paths:

- supplemental-first loss before main battery;
- survival-synergy hit mitigation amount;
- impact-guard hit mitigation amount;
- last-stand precedence;
- multiplayer downed precedence;
- solo soul-reboot precedence;
- non-hit zero-battery run death.

Evidence pointer:

```text
HEAD=ba5ef85539a0146935e933103989533f7848bef0
Gameplay checks=32171862921 PASS
```

Still not independently proven:

- exact stored-soul drain multiplier ordering relative to supplemental absorption as an isolated amount contract;
- exact intermediate HUD ticker / battery-audio ordering before survival fallback;
- whether those feedback-order details should remain authoritative contracts or become derived result publication during decomposition.

## Safe next step

Before changing `spendBattery()` production structure:

1. decide whether exact feedback ordering is observable behavior that must be frozen or an implementation detail that may become result-driven publication;
2. if it must be frozen, add one focused feedback-order characterization;
3. define the smallest `EnergyResult` facts needed by callers/feedback without moving gameplay values;
4. only then extract one transaction boundary mechanically;
5. preserve constants, survival precedence, and same-frame update order.
