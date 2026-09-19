#!/usr/bin/env python3
"""Honest articulated-body locomotion seam for Digital Breakdown.

The bootstrap policy is deliberately not an AI claim. It demonstrates the
interface a learned policy will inherit: observations in, normalized joint
targets out. No code writes root position, root velocity, or body orientation
after reset.
"""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import asdict, dataclass
from pathlib import Path
import time

import mujoco
import numpy as np


HERE = Path(__file__).resolve().parent
MODEL_PATH = HERE / "digital_breakdown_humanoid.xml"
CONTROL_HZ = 30
RESET_ANGULAR_VELOCITY = 0.08


@dataclass(frozen=True)
class Observation:
    time: float
    root_height: float
    root_x: float
    forward_speed: float
    up_x: float
    up_y: float
    up_z: float
    angular_velocity_x: float
    angular_velocity_y: float
    left_contact: float
    right_contact: float
    joint_position: tuple[float, ...]
    joint_velocity: tuple[float, ...]


@dataclass(frozen=True)
class Result:
    mode: str
    seconds: float
    desired_speed: float
    start_height: float
    final_height: float
    minimum_height: float
    distance: float
    mean_forward_speed: float
    upright_fraction: float
    left_contact_fraction: float
    right_contact_fraction: float
    fell: bool


def observe(model: mujoco.MjModel, data: mujoco.MjData) -> Observation:
    # MuJoCo free-joint quaternion is w,x,y,z. Third column, third row of its
    # rotation matrix is the pelvis local-up alignment with world up.
    rotation = np.empty(9, dtype=np.float64)
    mujoco.mju_quat2Mat(rotation, data.qpos[3:7])
    ground_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_GEOM, "ground")
    left_foot_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_GEOM, "left_foot_geom")
    right_foot_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_GEOM, "right_foot_geom")
    contact_pairs = {
        frozenset((int(data.contact[index].geom1), int(data.contact[index].geom2)))
        for index in range(data.ncon)
    }
    return Observation(
        time=float(data.time),
        root_height=float(data.qpos[2]),
        root_x=float(data.qpos[0]),
        forward_speed=float(data.qvel[0]),
        up_x=float(rotation[2]),
        up_y=float(rotation[5]),
        up_z=float(rotation[8]),
        angular_velocity_x=float(data.qvel[3]),
        angular_velocity_y=float(data.qvel[4]),
        left_contact=float(frozenset((ground_id, left_foot_id)) in contact_pairs),
        right_contact=float(frozenset((ground_id, right_foot_id)) in contact_pairs),
        joint_position=tuple(float(value) for value in data.qpos[7:]),
        joint_velocity=tuple(float(value) for value in data.qvel[6:]),
    )


class BootstrapPolicy:
    """A deterministic scaffold, replaceable by an exported learned policy."""

    def __init__(self, model: mujoco.MjModel) -> None:
        self.names = [
            mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_ACTUATOR, index)
            for index in range(model.nu)
        ]

    def action(self, observation: Observation, desired_speed: float, walking: bool) -> np.ndarray:
        targets = {name: 0.0 for name in self.names}
        if walking:
            frequency = 1.55 + 0.45 * min(1.0, abs(desired_speed))
            phase = observation.time * frequency * math.tau
            amplitude = 0.34 * min(1.0, abs(desired_speed) / 0.75)
            direction = 1.0 if desired_speed >= 0.0 else -1.0
            for side, offset in (("left", 0.0), ("right", math.pi)):
                swing = math.sin(phase + offset) * amplitude * direction
                lift = max(0.0, math.sin(phase + offset)) * abs(amplitude)
                targets[f"{side}_hip_pitch_motor"] = -swing
                targets[f"{side}_knee_motor"] = lift * 1.25
                targets[f"{side}_ankle_pitch_motor"] = swing * 0.42 - lift * 0.25
            # Arms are passive-looking counterweights, still force-driven.
            targets["left_shoulder_pitch_motor"] = math.sin(phase + math.pi) * amplitude * 0.65
            targets["right_shoulder_pitch_motor"] = math.sin(phase) * amplitude * 0.65

        # Normalize physical joint targets into the actuator control ranges.
        action = np.zeros(len(self.names), dtype=np.float64)
        for index, name in enumerate(self.names):
            action[index] = targets[name]
        return action


def render_frame(model: mujoco.MjModel, data: mujoco.MjData, path: Path) -> None:
    from PIL import Image

    path.parent.mkdir(parents=True, exist_ok=True)
    renderer = mujoco.Renderer(model, height=540, width=960)
    camera = mujoco.MjvCamera()
    camera.lookat[:] = (float(data.qpos[0]), 0.0, 0.8)
    camera.distance = 3.4
    camera.azimuth = 145
    camera.elevation = -13
    renderer.update_scene(data, camera=camera)
    Image.fromarray(renderer.render()).save(path)
    renderer.close()


def simulate(
    mode: str,
    seconds: float,
    desired_speed: float,
    render_path: Path | None = None,
) -> tuple[Result, Observation]:
    model = mujoco.MjModel.from_xml_path(str(MODEL_PATH))
    data = mujoco.MjData(model)
    # The ragdoll probe receives one small reset perturbation so broad feet and
    # perfect numerical symmetry cannot masquerade as active balance. This is
    # initial state only, never a per-step root correction.
    if mode == "fall":
        data.qvel[4] = RESET_ANGULAR_VELOCITY
    mujoco.mj_forward(model, data)
    if mode == "fall":
        # Zero is a valid target for a position servo, not "motor off".
        # Disable actuation inside MuJoCo so this probe is an honest ragdoll.
        model.opt.disableflags |= int(mujoco.mjtDisableBit.mjDSBL_ACTUATION)
    policy = BootstrapPolicy(model)
    control_stride = round((1.0 / CONTROL_HZ) / model.opt.timestep)
    frame_count = round(seconds / model.opt.timestep)
    samples: list[Observation] = []

    initial = observe(model, data)
    for frame in range(frame_count):
        current = observe(model, data)
        if mode == "fall":
            data.ctrl[:] = 0.0
        elif frame % control_stride == 0:
            data.ctrl[:] = policy.action(current, desired_speed, walking=mode == "walk")
        mujoco.mj_step(model, data)
        if frame % control_stride == 0:
            samples.append(observe(model, data))

    final = observe(model, data)
    if render_path is not None:
        render_frame(model, data, render_path)
    heights = np.asarray([sample.root_height for sample in samples])
    result = Result(
        mode=mode,
        seconds=seconds,
        desired_speed=desired_speed,
        start_height=initial.root_height,
        final_height=final.root_height,
        minimum_height=float(heights.min()),
        distance=final.root_x - initial.root_x,
        mean_forward_speed=float(np.mean([sample.forward_speed for sample in samples])),
        upright_fraction=float(np.mean([sample.up_z > 0.65 for sample in samples])),
        left_contact_fraction=float(np.mean([sample.left_contact > 0.0 for sample in samples])),
        right_contact_fraction=float(np.mean([sample.right_contact > 0.0 for sample in samples])),
        fell=bool(final.root_height < 0.64 or final.up_z < 0.45),
    )
    return result, final


def run_viewer(mode: str, desired_speed: float) -> None:
    import mujoco.viewer

    model = mujoco.MjModel.from_xml_path(str(MODEL_PATH))
    data = mujoco.MjData(model)
    policy = BootstrapPolicy(model)
    control_stride = round((1.0 / CONTROL_HZ) / model.opt.timestep)
    frame = 0
    with mujoco.viewer.launch_passive(model, data) as viewer:
        while viewer.is_running():
            started = time.perf_counter()
            current = observe(model, data)
            if mode == "fall":
                data.ctrl[:] = 0.0
            elif frame % control_stride == 0:
                data.ctrl[:] = policy.action(current, desired_speed, walking=mode == "walk")
            mujoco.mj_step(model, data)
            viewer.sync()
            frame += 1
            time.sleep(max(0.0, model.opt.timestep - (time.perf_counter() - started)))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("fall", "stand", "walk"), default="stand")
    parser.add_argument("--seconds", type=float, default=5.0)
    parser.add_argument("--desired-speed", type=float, default=0.65)
    parser.add_argument("--render", type=Path)
    parser.add_argument("--viewer", action="store_true")
    args = parser.parse_args()

    if args.viewer:
        run_viewer(args.mode, args.desired_speed)
        return

    result, final = simulate(args.mode, args.seconds, args.desired_speed, args.render)
    if args.render:
        print(f"render={args.render.resolve()}")
    print(json.dumps({"result": asdict(result), "final_observation": asdict(final)}, indent=2))


if __name__ == "__main__":
    main()
