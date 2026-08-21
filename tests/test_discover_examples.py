from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
SCRIPTS = REPO / "scripts"
WORKFLOW = REPO / ".github/workflows/examples.yml"
sys.path.insert(0, str(SCRIPTS))

import discover_examples  # noqa: E402


IDF_NAMES = {
    "01_AXP2101",
    "02_lvgl_demo_v9",
    "03_esp-brookesia",
    "04_Immersive_block",
    "05_Spec_Analyzer",
}
ARDUINO_NAMES = {
    "01_HelloWorld",
    "02_GFX_AsciiTable",
    "03_LVGL_AXP2101_ADC_Data",
    "04_LVGL_QMI8658_ui",
    "05_LVGL_Widgets",
    "06_ES7210",
    "07_ES8311",
}
ARDUINO_FQBN = (
    "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,"
    "PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc"
)


class DiscoverExamplesTests(unittest.TestCase):
    def test_inventory_counts_and_library_exclusion(self) -> None:
        idf = discover_examples.discover_esp_idf(REPO)
        arduino = discover_examples.discover_arduino(REPO)
        self.assertEqual({item["name"] for item in idf}, IDF_NAMES)
        self.assertEqual({item["name"] for item in arduino}, ARDUINO_NAMES)
        self.assertEqual(len(idf), 5)
        self.assertEqual(len(arduino), 7)
        self.assertTrue(all("/libraries/" not in item["path"] for item in arduino))

    def test_name_path_and_legacy_selectors(self) -> None:
        idf_entry = {"name": "01_AXP2101", "path": "examples/esp-idf/01_AXP2101"}
        self.assertTrue(discover_examples.selector_matches(idf_entry, "01_AXP2101"))
        self.assertTrue(discover_examples.selector_matches(
            idf_entry, "examples/esp-idf/01_AXP2101"
        ))
        self.assertTrue(discover_examples.selector_matches(
            idf_entry, "examples/ESP-IDF-v5.5/01_AXP2101"
        ))

        arduino_entry = {
            "name": "01_HelloWorld",
            "path": "examples/arduino/examples/01_HelloWorld",
            "ino": "01_HelloWorld.ino",
        }
        self.assertTrue(discover_examples.selector_matches(arduino_entry, "01_HelloWorld.ino"))
        self.assertTrue(discover_examples.selector_matches(
            arduino_entry,
            "examples/Arduino-v3.3.5/examples/01_HelloWorld",
        ))

    def test_multiple_selectors(self) -> None:
        entries = discover_examples.discover_esp_idf(REPO)
        selected = [
            item for item in entries
            if discover_examples.selector_matches(
                item,
                "01_AXP2101,examples/esp-idf/02_lvgl_demo_v9",
            )
        ]
        self.assertEqual({item["name"] for item in selected}, {
            "01_AXP2101", "02_lvgl_demo_v9"
        })

    def test_cli_defaults_produce_the_required_matrix(self) -> None:
        idf = subprocess.run(
            [
                sys.executable,
                str(SCRIPTS / "discover_examples.py"),
                "--repo", str(REPO),
                "--surface", "esp-idf",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        arduino = subprocess.run(
            [
                sys.executable,
                str(SCRIPTS / "discover_examples.py"),
                "--repo", str(REPO),
                "--surface", "arduino",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        idf_matrix = json.loads(idf.stdout)["include"]
        arduino_matrix = json.loads(arduino.stdout)["include"]
        self.assertEqual(len(idf_matrix), 10)
        self.assertEqual({item["idf"] for item in idf_matrix}, {"v5.5.5", "v6.0.2"})
        self.assertEqual(len(arduino_matrix), 7)
        self.assertEqual({item["core"] for item in arduino_matrix}, {"3.3.11"})
        self.assertEqual(discover_examples.ARDUINO_FQBN, ARDUINO_FQBN)
        self.assertEqual({item["fqbn"] for item in arduino_matrix}, {
            ARDUINO_FQBN
        })
        self.assertTrue(all(
            option in discover_examples.ARDUINO_FQBN
            for option in (
                "FlashSize=16M",
                "PSRAM=opi",
                "PartitionScheme=app3M_fat9M_16MB",
                "USBMode=hwcdc",
                "CDCOnBoot=cdc",
            )
        ))

    def test_workflow_uses_the_tested_arduino_fqbn_default(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        discover_job = workflow.split("  discover-arduino:\n", 1)[1].split(
            "  build-arduino:\n", 1
        )[0]
        self.assertIn("python3 scripts/discover_examples.py", discover_job)
        self.assertNotIn(
            "--fqbn",
            discover_job,
            "the workflow must not override the tested Arduino FQBN default",
        )
        self.assertIn('--fqbn "${{ matrix.fqbn }}"', workflow)


if __name__ == "__main__":
    unittest.main()
