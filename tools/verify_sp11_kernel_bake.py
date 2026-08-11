#!/usr/bin/env python3
"""Fail a SP11 kernel bake if its required Wi-Fi RF-kill fix is missing."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


ATH12K_RELATIVE = Path("drivers/net/wireless/ath/ath12k/core.c")
DTS_RELATIVE = Path("arch/arm64/boot/dts/qcom/x1-microsoft-denali.dtsi")
WIFI_NODE = "/soc@0/pci@1c08000/pcie@0/wifi@0"

ATH12K_HOOK = re.compile(
    r'of_property_read_bool\(ab->dev->of_node,\s*"disable-rfkill"\)'
)
WIFI_DT_POLICY = re.compile(
    r"&pcie4_port0\s*\{.*?wifi@0\s*\{.*?disable-rfkill\s*;",
    re.DOTALL,
)


def verify_source(source: Path) -> dict:
    driver = source / ATH12K_RELATIVE
    dtsi = source / DTS_RELATIVE
    if not driver.is_file():
        raise ValueError(f"missing ath12k source: {driver}")
    if not dtsi.is_file():
        raise ValueError(f"missing SP11 board source: {dtsi}")

    driver_text = driver.read_text(encoding="utf-8", errors="replace")
    dtsi_text = dtsi.read_text(encoding="utf-8", errors="replace")
    if not ATH12K_HOOK.search(driver_text):
        raise ValueError("ath12k does not honor the DT disable-rfkill property")
    if not WIFI_DT_POLICY.search(dtsi_text):
        raise ValueError("SP11 WCN7850 node does not contain disable-rfkill")

    return {
        "ath12k_dt_disable_rfkill_hook": True,
        "sp11_wcn7850_disable_rfkill_property": True,
    }


def verify_dtb(dtb: Path, fdtget: str = "fdtget") -> dict:
    if not dtb.is_file():
        raise ValueError(f"missing compiled DTB: {dtb}")
    result = subprocess.run(
        [fdtget, str(dtb), WIFI_NODE, "disable-rfkill"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode:
        detail = result.stderr.strip() or "property not found"
        raise ValueError(f"compiled DTB lost WCN7850 disable-rfkill: {detail}")
    return {"compiled_dtb_disable_rfkill_property": True}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="full Linux source tree")
    parser.add_argument("--dtb", type=Path, help="freshly compiled final DTB")
    args = parser.parse_args()

    checks = verify_source(args.source)
    if args.dtb:
        checks.update(verify_dtb(args.dtb))
    print(json.dumps({"accepted": True, "checks": checks}, indent=2,
                     sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
