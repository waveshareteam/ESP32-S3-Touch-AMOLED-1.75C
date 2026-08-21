from __future__ import annotations

import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
PIN_CONFIG = REPO / "examples/arduino/libraries/Mylibrary/pin_config.h"
DEFINE = re.compile(r"^\s*#define\s+(\w+)\s+(\d+)\s*$", re.MULTILINE)


class HardwareContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.definitions = {
            name: int(value)
            for name, value in DEFINE.findall(PIN_CONFIG.read_text(encoding="utf-8"))
        }

    def test_schematic_backed_arduino_pin_contract(self) -> None:
        expected = {
            "LCD_SDIO0": 4,
            "LCD_SDIO1": 5,
            "LCD_SDIO2": 6,
            "LCD_SDIO3": 7,
            "LCD_SCLK": 38,
            "LCD_RESET": 1,
            "LCD_CS": 12,
            "LCD_WIDTH": 466,
            "LCD_HEIGHT": 466,
            "IIC_SDA": 15,
            "IIC_SCL": 14,
            "TP_INT": 11,
            "TP_RST": 2,
            "PIN_ES7210_BCLK": 9,
            "PIN_ES7210_LRCK": 45,
            "PIN_ES7210_DIN": 10,
            "PIN_ES7210_MCLK": 16,
            "PIN_ES8311_DOUT": 8,
            "PA": 46,
        }
        for macro, expected_value in expected.items():
            self.assertIn(macro, self.definitions, f"{macro} must be defined in {PIN_CONFIG}")
            self.assertEqual(
                self.definitions[macro],
                expected_value,
                f"{macro} must match the schematic-backed hardware contract",
            )

    def test_lcd_and_touch_reset_are_distinct(self) -> None:
        self.assertEqual(self.definitions.get("LCD_RESET"), 1, "LCD_RESET must be GPIO1")
        self.assertEqual(self.definitions.get("TP_RST"), 2, "TP_RST must be GPIO2")
        self.assertNotEqual(
            self.definitions.get("LCD_RESET"),
            self.definitions.get("TP_RST"),
            "LCD_RESET and TP_RST must remain distinct schematic signals",
        )


if __name__ == "__main__":
    unittest.main()
