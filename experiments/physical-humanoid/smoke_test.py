#!/usr/bin/env python3
"""Cheap behavioral gates for the isolated physical humanoid experiment."""

from physical_walker import simulate


def main() -> None:
    fall, _ = simulate("fall", 5.0, 0.0)
    assert fall.fell, f"motors-off body was unnaturally supported: {fall}"

    stand, _ = simulate("stand", 3.0, 0.0)
    assert stand.final_height > fall.final_height + 0.15, (
        "joint motors did not materially improve support over ragdoll: "
        f"fall={fall.final_height:.3f}, stand={stand.final_height:.3f}"
    )
    assert not stand.fell, f"bootstrap controller cannot yet support a three-second stance: {stand}"

    walk, _ = simulate("walk", 4.0, 0.65)
    # This is intentionally not yet a locomotion success gate. It proves the
    # command does not directly move the root; a learned controller must earn
    # positive displacement without weakening the honest fall test.
    assert abs(walk.distance) < 8.0, f"physically implausible root displacement: {walk.distance:.3f}"

    print("PHYSICAL_HUMANOID_SMOKE_OK")
    print(f"fall_height={fall.final_height:.3f}")
    print(f"stand_height={stand.final_height:.3f}")
    print(f"walk_distance={walk.distance:.3f}")
    print(f"walk_upright_fraction={walk.upright_fraction:.3f}")


if __name__ == "__main__":
    main()
