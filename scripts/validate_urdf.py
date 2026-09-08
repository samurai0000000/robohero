#!/usr/bin/env python3
#
# validate_urdf.py
#
# Copyright (C) 2026, Charles Chiou
#
# Validate RoboHero URDF model, check kinematic tree, masses, joint channels,
# and cross-validate against canonical model/calibration.json.
#

import json
import math
import pathlib
import sys
import xml.etree.ElementTree as ET


def print_tree(tree, link, prefix="", is_last=True):
    connector = "└── " if is_last else "├── "
    print(f"{prefix}{connector}{link}")
    children = tree.get(link, [])
    new_prefix = prefix + ("    " if is_last else "│   ")
    for i, (child_link, joint_name, axis) in enumerate(children):
        is_child_last = i == len(children) - 1
        print(f"{new_prefix}  ({joint_name}) [axis={axis}]")
        print_tree(tree, child_link, new_prefix, is_child_last)


def main():
    root_dir = pathlib.Path(__file__).resolve().parent.parent
    urdf_path = root_dir / "model" / "robohero.urdf"
    calib_path = root_dir / "model" / "calibration.json"

    if not urdf_path.exists():
        sys.exit(f"Error: URDF file not found at {urdf_path}")
    if not calib_path.exists():
        sys.exit(f"Error: Calibration file not found at {calib_path}")

    print(f"Validating URDF: {urdf_path}")
    print(f"Validating Calibration: {calib_path}")

    try:
        calib_data = json.loads(calib_path.read_text(encoding="utf-8"))
    except Exception as e:
        sys.exit(f"Error parsing JSON in {calib_path}: {e}")

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
        lower_val = 0.0
        upper_val = 0.0
        if limit is not None:
            lower_val = float(limit.attrib.get("lower", 0.0))
            upper_val = float(limit.attrib.get("upper", 0.0))
            limits_str = f"[{math.degrees(lower_val):.1f}°, {math.degrees(upper_val):.1f}°]"

        joints[name] = {
            "type": jtype,
            "parent": parent,
            "child": child,
            "axis": axis,
            "lower": lower_val,
            "upper": upper_val,
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

    # Verify Hardware Channels against calibration.json
    print("\n--- Hardware Channel & Calibration Verification ---")
    channels = calib_data.get("channels", {})
    if len(channels) != 17:
        sys.exit(f"Error: Expected 17 channels in calibration.json, found {len(channels)}")

    found_joint_names = set(joints.keys())
    missing_joints = []
    limit_mismatches = []
    deg_range_mismatches = []
    scale_mismatches = []
    sign_errors = []

    for ch in range(17):
        ch_key = str(ch)
        if ch_key not in channels:
            missing_joints.append((ch, f"ch_{ch}", "Missing in calibration.json"))
            continue

        ch_info = channels[ch_key]
        expected_name = ch_info["name"]
        desc = ch_info.get("label", expected_name)

        if expected_name in found_joint_names:
            j_info = joints[expected_name]
            cal_limits = ch_info.get("urdf_limits", {})
            cal_lower = cal_limits.get("lower", 0.0)
            cal_upper = cal_limits.get("upper", 0.0)

            # 1. Check if URDF XML limit matches calibration.json limits
            if abs(j_info["lower"] - cal_lower) > 0.001 or abs(j_info["upper"] - cal_upper) > 0.001:
                limit_mismatches.append((
                    ch, expected_name,
                    (j_info["lower"], j_info["upper"]),
                    (cal_lower, cal_upper)
                ))

            # 2. Check if degree_range in JSON matches urdf_limits (radians to degrees)
            deg_range = ch_info.get("degree_range", [])
            if len(deg_range) == 2:
                exp_deg_lower = round(math.degrees(cal_lower), 1)
                exp_deg_upper = round(math.degrees(cal_upper), 1)
                if abs(deg_range[0] - exp_deg_lower) > 0.2 or abs(deg_range[1] - exp_deg_upper) > 0.2:
                    deg_range_mismatches.append((
                        ch, expected_name, deg_range, [exp_deg_lower, exp_deg_upper]
                    ))

            # 3. Check rad_per_pwm and deg_per_pwm consistency
            rad_per_pwm = ch_info.get("rad_per_pwm", 0.0)
            deg_per_pwm = ch_info.get("deg_per_pwm", 0.0)
            expected_deg_scale = rad_per_pwm * (180.0 / math.pi)
            if abs(deg_per_pwm - expected_deg_scale) > 0.01:
                scale_mismatches.append((ch, expected_name, deg_per_pwm, expected_deg_scale))

            # 4. Check sign validity (+1.0 or -1.0)
            sign = ch_info.get("sign", 0.0)
            if sign not in (1.0, -1.0):
                sign_errors.append((ch, expected_name, sign))

            print(f"  [Ch {ch:2d}] {desc:<24} -> {expected_name:<28} (axis={j_info['axis']}, limits={j_info['limits']}, sign={sign:+3.1f})")
        else:
            missing_joints.append((ch, expected_name, desc))

    if missing_joints:
        print("\nERROR: Missing hardware joints in URDF:")
        for ch, name, desc in missing_joints:
            print(f"  - Ch {ch}: {name} ({desc})")
        sys.exit(1)

    if limit_mismatches:
        print("\nERROR: Limit discrepancies between URDF XML and calibration.json:")
        for ch, name, urdf_lim, cal_lim in limit_mismatches:
            print(f"  - Ch {ch} ({name}): URDF XML={urdf_lim} vs Calib={cal_lim}")
        sys.exit(1)

    if deg_range_mismatches:
        print("\nERROR: degree_range mismatch against urdf_limits in calibration.json:")
        for ch, name, actual_deg, expected_deg in deg_range_mismatches:
            print(f"  - Ch {ch} ({name}): degree_range={actual_deg} vs expected={expected_deg}")
        sys.exit(1)

    if scale_mismatches:
        print("\nERROR: Scale factor discrepancy (deg_per_pwm vs rad_per_pwm):")
        for ch, name, act_deg, exp_deg in scale_mismatches:
            print(f"  - Ch {ch} ({name}): deg_per_pwm={act_deg} vs expected={exp_deg:.4f}")
        sys.exit(1)

    if sign_errors:
        print("\nERROR: Invalid kinematic sign multiplier (must be +1.0 or -1.0):")
        for ch, name, sign in sign_errors:
            print(f"  - Ch {ch} ({name}): sign={sign}")
        sys.exit(1)

    print("\nAll 17 hardware channels successfully mapped and verified!")

    # Print Kinematic Tree
    print("\n--- Kinematic Tree ---")
    print_tree(kin_tree, root_link)
    print("\nURDF and Calibration Validation PASSED.")


if __name__ == "__main__":
    main()
