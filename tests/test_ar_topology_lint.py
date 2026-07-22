import unittest

from tools.ar_topology_lint import duplicate_instances, parse_module_instances


def tuple_block(name: str, instance_id: int) -> str:
    return f"""
    '{name}:tuple0' {{
        tokens '{name}'
        tuples {{
            2_word {{
                token201 {instance_id}
                token208 {instance_id}
            }}
        }}
    }}
    """


def widget_section(*names: str) -> str:
    widgets = "\n".join(
        f"""
        '{name}' {{
            data '{name}:tuple0'
        }}
        """
        for name in names
    )
    return f"SectionWidget {{\n{widgets}\n}}\n"


class TopologyLintTests(unittest.TestCase):
    def test_ignores_vendor_token_declaration_and_manifest(self) -> None:
        text = """
        SectionVendorTokens.'audioreach tokens' {
            token201 201
        }
        """ + widget_section("stream0.module0") + tuple_block(
            "manifest", 0x6000
        ) + tuple_block("stream0.module0", 0x6000)

        instances = parse_module_instances(text)

        self.assertEqual([item.instance_id for item in instances], [0x6000])

    def test_unique_instance_ids_pass(self) -> None:
        instances = parse_module_instances(
            widget_section("stream0.module0", "stream0.module1")
            + tuple_block("stream0.module0", 0x6000)
            + tuple_block("stream0.module1", 0x6001)
        )

        self.assertEqual(duplicate_instances(instances), {})

    def test_duplicate_instance_id_reports_both_modules(self) -> None:
        instances = parse_module_instances(
            widget_section("stream0.msiir0", "stream2.logger1")
            + tuple_block("stream0.msiir0", 0x6020)
            + tuple_block("stream2.logger1", 0x6020)
        )

        duplicates = duplicate_instances(instances)

        self.assertEqual(set(duplicates), {0x6020})
        self.assertEqual(
            [item.name for item in duplicates[0x6020]],
            ["stream0.msiir0", "stream2.logger1"],
        )


if __name__ == "__main__":
    unittest.main()
