#!/usr/bin/env python3
#
# validate_urdf.py
#
# Copyright (C) 2026, Charles Chiou
#
# Validate RoboHero URDF model, check kinematic tree, masses, and joint channels.
#

import math
import pathlib
import sys
import xml.etree.ElementTree as ET

# Hardware channel to joint name mapping for RoboHero
EXPECTED_JOINTS = {
    0: ("left_ankle_roll_joint", "Left Ankle Roll"),
    1: ("left_ankle_pitch_joint", "Left Ankle Pitch"),
    2: ("left_knee_pitch_joint", "Left Knee Pitch"),
    3: ("left_hip_pitch_joint", "Left Hip Pitch"),
    4: ("left_hip_roll_joint", "Left Hip Roll"),
    5: ("left_shoulder_pitch_joint", "Left Shoulder Pitch"),
    6: ("left_shoulder_roll_joint", "Left Shoulder Roll"),
    7: ("left_elbow_joint", "Left Elbow"),
    8: ("right_elbow_joint", "Right Elbow"),
    9: ("right_shoulder_roll_joint", "Right Shoulder Roll"),
    10: ("right_shoulder_pitch_joint", "Right Shoulder Pitch"),
    11: ("right_hip_roll_joint", "Right Hip Roll"),
    12: ("right_hip_pitch_joint", "Right Hip Pitch"),
    13: ("right_knee_pitch_joint", "Right Knee Pitch"),
    14: ("right_ankle_pitch_joint", "Right Ankle Pitch"),
    15: ("right_ankle_roll_joint", "Right Ankle Roll"),
    16: ("head_yaw_joint", "Head Yaw (GPIO12)"),
}


def print_tree(tree, link, prefix="", is_last=True):
    connector = "└── " if is_last else "├── "
    print(f"{prefix}{connector}{link}")
    children = tree.get(link, [])
    new_prefix = prefix + ("    " if is_last else "│   ")
    for i, (child_link, joint_name, axis) in enumerate(children):
        is_child_last = i == len(children) - 1
        joint_info = f" [{joint_name}, axis={axis}]"
        print(f"{new_prefix}  ({joint_name})")
        print_tree(tree, child_link, new_prefix, is_child_last)


def main():
    root_dir = pathlib.Path(__file__).resolve().parent.parent
    urdf_path = root_dir / "urdf" / "robohero.urdf"

    if not urdf_path.exists():
        sys.exit(f"Error: URDF file not found at {urdf_path}")

    print(f"Validating URDF: {urdf_path}")

    try:
        tree = ET.parse(urdf_path)
        root = tree.getroot()
    except Exception as e:
        sys.exit(f"Error parsing XML in {urdf_path}: {e}")

    if root.tag != "robot":
        sys.exit("Error: Root element is not <robot>")

    robot_name = root.attrib.get("name", "unnamed")
    print(f"Robot Name: {robot_name}")

    links = {}
    total_mass = 0.0
    for link in root.findall("link"):
        name = link.attrib.get("name")
        inertial = link.find("inertial")
        mass_val = 0.0
        if inertial is not None:
            mass_elem = inertial.find("mass")
            if mass_elem is not None:
                mass_val = float(mass_elem.attrib.get("value", 0.0))
        links[name] = {"mass": mass_val}
        total_mass += mass_val

    joints = {}
    parent_map = {}
    child_map = {}
    kin_tree = {}

    for joint in root.findall("joint"):
        name = joint.attrib.get("name")
        jtype = joint.attrib.get("type")
        parent = joint.find("parent").attrib.get("link")
        child = joint.find("child").attrib.get("link")
        axis_elem = joint.find("axis")
        axis = axis_elem.attrib.get("xyz", "1 0 0") if axis_elem is not None else "1 0 0"
        limit = joint.find("limit")
        limits_str = ""
        if limit is not None:
            lower = float(limit.attrib.get("lower", 0.0))
            upper = float(limit.attrib.get("upper", 0.0))
            limits_str = f"[{math.degrees(lower):.1f}°, {math.degrees(upper):.1f}°]"

        joints[name] = {
            "type": jtype,
            "parent": parent,
            "child": child,
            "axis": axis,
            "limits": limits_str,
        }
        parent_map[child] = (parent, name)
        kin_tree.setdefault(parent, []).append((child, name, axis))

    # Find root link (link with no parent)
    all_links = set(links.keys())
    child_links = set(parent_map.keys())
    root_candidates = all_links - child_links

    if len(root_candidates) != 1:
        sys.exit(f"Error: Expected exactly 1 root link, found {len(root_candidates)}: {root_candidates}")

    root_link = list(root_candidates)[0]

    print("\n--- Model Summary ---")
    print(f"Root Link: {root_link}")
    print(f"Total Links: {len(links)}")
    print(f"Total Joints: {len(joints)}")
    print(f"Total Model Mass: {total_mass * 1000.0:.1f} g ({total_mass:.3f} kg)")

    # Verify Hardware Channels
    print("\n--- Hardware Channel Verification ---")
    found_joint_names = set(joints.keys())
    missing_joints = []
    for ch in range(17):
        expected_name, desc = EXPECTED_JOINTS[ch]
        if expected_name in found_joint_names:
            j_info = joints[expected_name]
            print(f"  [Ch {ch:2d}] {desc:<24} -> {expected_name:<28} (axis={j_info['axis']}, limits={j_info['limits']})")
        else:
            missing_joints.append((ch, expected_name, desc))

    if missing_joints:
        print("\nERROR: Missing hardware joints:")
        for ch, name, desc in missing_joints:
            print(f"  - Ch {ch}: {name} ({desc})")
        sys.exit(1)

    print("\nAll 17 hardware channels successfully mapped!")

    # Print Kinematic Tree
    print("\n--- Kinematic Tree ---")
    print_tree(kin_tree, root_link)
    print("\nURDF Validation PASSED.")


if __name__ == "__main__":
    main()
