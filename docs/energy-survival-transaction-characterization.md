# Energy / Survival Transaction Characterization

Base observed: `agent/ownership-contracts-settings` after `4af4a613578ab6f82d571d86e34f54baef16f489`.

Status: observed current behavior. This document is descriptive. It is not permission to change gameplay values or precedence.

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

## 1. Hit mitigation happens before resource absorption

For `BatteryReason::Hit` only:

- survival synergy reduces incoming amount;
- active impact guard further scales the hit;
- the mitigated amount is what proceeds into the normal battery-cost path.

This means impact guard/survival are damage-policy modifiers, not post-damage refunds.

## 2. Stored souls alter ordinary battery drain before supplemental absorption

The cost is multiplied by `batteryDrainMultiplier()`, which depends on stored soul count, before supplemental power is consumed.

Current interpretation:

```text
requested amount
→ reason-specific mitigation
→ stored-soul efficiency
→ supplemental battery
→ main battery
```

Do not swap supplemental absorption and the soul-efficiency multiplier without treating that as a gameplay change.

## 3. Supplemental battery is first-loss energy

`consumeSupplementalBattery()` absorbs as much of the scaled cost as possible before main battery changes.

If supplemental value is exhausted, `clearActivePowerups()` clears the active supplemental-power state.

This is the main evidence that `EnergyState` and `PlayerState::battery` are separate resource layers, not interchangeable fields.

## 4. Feedback currently occurs before zero-battery survival fallback

After main battery subtraction:

- non-continuous requests publish an energy ticker;
- battery threshold audio is evaluated;
- only then does the transaction evaluate last stand / downed / soul reboot / death.

A future authoritative transaction object may separate mutation from feedback publication, but observable feedback timing must be characterized before reordering.

## 5. Last stand has first claim on a zero-battery hit

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

This precedence is gameplay-significant.

## 6. Multiplayer hit zero resolves to downed before death

If battery remains zero after last-stand evaluation and reason is `Hit` while multiplayer is enabled:

- player becomes downed;
- bleedout timer becomes 15 seconds;
- revive charge resets;
- velocity/jump velocity clear;
- player lifecycle actions clear;
- ticker becomes `SIGNAL DOWN`;
- transaction returns failure;
- immediate run death does not occur.

Bleedout resolution is handled later by `updateBattery()`.

## 7. Solo hit zero may consume one stored soul for reboot

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

The room-advance policy explicitly resets `soloSoulRebootUsed`, so this fallback is currently room-scoped.

## 8. Remaining zero-battery cases trigger run death

If no prior survival branch claims the zero-battery state, `triggerRunDeath()` owns final run-death mutation and presentation setup.

This includes non-hit battery exhaustion.

## Ownership target

The eventual shape should preserve the above precedence while making authority explicit:

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

Do not introduce this type until focused tests cover the survival branches.

## Test status

Already covered indirectly:

- ordinary battery drain and gain are exercised throughout gameplay/progression tests;
- room transition contracts now cover action-runtime clearing around discharge state.

Not yet covered by a focused contract test:

- supplemental-first precedence for a single request;
- hit mitigation amount;
- last stand precedence;
- multiplayer downed precedence;
- solo soul reboot precedence;
- non-hit zero-battery death;
- exact feedback ordering around those branches.

These remain `OBSERVED`, not independently `PROVEN` by a focused survival test.

## Safe next step

Before extracting `spendBattery()`:

1. identify the least brittle public/runtime path for producing an authoritative `Hit` request;
2. add focused survival transaction tests through that path;
3. prove current branch precedence;
4. only then extract mutation/result from presentation feedback;
5. preserve constants and same-frame order.
