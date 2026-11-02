import copy
import importlib.util
import json
import re
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
audit_spec = importlib.util.spec_from_file_location("rpc_schema_audit", ROOT / "tools/audit_rpc_contract.py")
audit = importlib.util.module_from_spec(audit_spec)
audit_spec.loader.exec_module(audit)
generator_spec = importlib.util.spec_from_file_location("rpc_schema_generator", ROOT / "tools/generate_rpc_schema.py")
generator = importlib.util.module_from_spec(generator_spec)
with patch.dict(sys.modules, {"audit_rpc_contract": audit}):
    generator_spec.loader.exec_module(generator)


class SchemaTests(unittest.TestCase):
    def setUp(self):
        self.registry = audit.Load_Json(ROOT / "specs/rpc/methods.json")
        self.contracts = audit.Load_Json(ROOT / "specs/rpc/contracts.json")
        self.policies = audit.Load_Json(ROOT / "specs/rpc/field_policies.json")
        self.schema = generator.Build_Schema(self.registry, self.contracts, self.policies)

    def test_every_method_has_distinct_request_and_response_entrypoints(self):
        self.assertEqual(len(self.schema["oneOf"]), 60)
        self.assertEqual(len(self.schema["x-rpc-methods"]), 60)
        self.assertFalse(self.schema["x-rpc-runtimeEnabled"])
        for method in self.schema["x-rpc-methods"]:
            self.assertEqual([method["requestType"], method["resultType"]],
                             self.contracts["methods"][method["name"]])

    def test_all_references_resolve_inside_the_bundle(self):
        def Visit(value):
            if isinstance(value, dict):
                if "$ref" in value:
                    reference = value["$ref"]
                    self.assertTrue(reference.startswith("#/$defs/"))
                    self.assertIn(reference.rsplit("/", 1)[1], self.schema["$defs"])
                for item in value.values():
                    Visit(item)
            elif isinstance(value, list):
                for item in value:
                    Visit(item)
        Visit(self.schema)

    def test_visibility_units_and_nonstandard_constraints_are_preserved(self):
        definitions = self.schema["$defs"]
        self.assertEqual(definitions["System"]["properties"]["owner"]["x-rpc-policy"], "system-owner")
        self.assertEqual(definitions["Coord"]["x-unit"], "map")
        self.assertEqual(definitions["Id"]["x-maxUtf8Bytes"], 128)
        self.assertEqual(definitions["Key"]["x-maxUtf8Bytes"], 384)
        self.assertEqual(definitions["Counter"]["x-maxDecimal"], "18446744073709551615")
        self.assertIn("not standard JSON Schema assertions", self.schema["description"])

    def test_schema_is_deterministic_and_source_changes_change_its_identity(self):
        self.assertEqual(self.schema, generator.Build_Schema(self.registry, self.contracts, self.policies))
        changed = copy.deepcopy(self.contracts)
        changed["types"]["Id"]["maxBytes"] = 127
        self.assertNotEqual(self.schema["x-rpc-sourceHash"],
                            generator.Build_Schema(self.registry, changed, self.policies)["x-rpc-sourceHash"])

    def test_human_reference_consumes_only_the_serialized_schema(self):
        schema = json.loads(json.dumps(self.schema))
        reference = generator.Render_Markdown(schema)
        self.assertIn("do not edit", reference)
        self.assertIn("disabled by default", reference)
        for method in self.registry["methods"]:
            self.assertIn(method["name"], reference)
        schema["$defs"]["Id"]["description"] = "Schema-only documentation change"
        self.assertIn("Schema-only documentation change", generator.Render_Markdown(schema))
        self.assertNotIn("Schema-only documentation change", reference)

    def test_cpp_contract_uses_bounded_independent_literals_and_preserves_json(self):
        contracts = copy.deepcopy(self.contracts)
        contracts["types"]["LongLiteralRegression"] = {
            "kind":"enum", "values":['quotes" backslash\\ unicode\u2603 ' * 3000]}
        source = generator.Render_Cpp(self.registry, contracts)
        chunks = re.findall(r'R"rpc_types\((.*?)\)rpc_types",', source, re.DOTALL)
        self.assertTrue(chunks)
        self.assertTrue(all(len(chunk.encode("ascii")) <= 4096 for chunk in chunks))
        encoded = "".join(chunks)
        self.assertGreater(len(encoded), 65535)
        self.assertEqual(encoded, json.dumps(contracts["types"], ensure_ascii=True, separators=(",", ":")))
        self.assertIn('rpc_contract_data.h', source)
        self.assertNotIn('.inc', source)

    def test_reference_escapes_table_and_html_content(self):
        self.assertEqual(generator.Cell("<script>|\n"), "&lt;script&gt;\\|<br>")

    def test_generation_replaces_complete_outputs_and_does_not_rewrite_unchanged_files(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "schema.json"
            generator.Write_Output(path, "first\n")
            self.assertEqual(path.read_text(), "first\n")
            with patch.object(generator.os, "replace") as replace:
                generator.Write_Output(path, "first\n")
                replace.assert_not_called()
            generator.Write_Output(path, "second\n")
            self.assertEqual(path.read_text(), "second\n")
            self.assertEqual(list(Path(directory).iterdir()), [path])

    def test_checked_in_schema_and_reference_are_current(self):
        result = subprocess.run([sys.executable, str(ROOT / "tools/generate_rpc_schema.py"), "--check"],
                                cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_check_detects_drift_without_rewriting_it(self):
        with tempfile.TemporaryDirectory() as directory:
            command = [sys.executable, str(ROOT / "tools/generate_rpc_schema.py"), "--output-dir", directory]
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True)
            path = Path(directory) / "reference.md"
            path.write_text("stale\n", encoding="utf-8")
            result = subprocess.run(command + ["--check"], cwd=ROOT, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("stale generated artifact", result.stderr)
            self.assertEqual(path.read_text(), "stale\n")

    @unittest.skipUnless(importlib.util.find_spec("jsonschema"), "optional jsonschema package not installed")
    def test_standard_json_schema_metaschema_and_request_validation(self):
        import jsonschema
        jsonschema.Draft202012Validator.check_schema(self.schema)
        validator = jsonschema.Draft202012Validator(self.schema)
        self.assertTrue(validator.is_valid({"jsonrpc":"2.0", "id":1, "method":"game.getSession"}))
        self.assertTrue(validator.is_valid({"jsonrpc":"2.0", "id":"s", "method":"game.getSystem", "params":{"id":"opaque"}}))
        for request in ({"jsonrpc":"2.0", "id":1, "method":"game.getSystem"},
                        {"jsonrpc":"2.0", "id":1, "method":"game.getSession", "params":{"viewerPlayerId":2}},
                        {"jsonrpc":"2.0", "id":True, "method":"game.getSession"},
                        {"jsonrpc":"2.0", "id":1, "method":"game.readMemory"}):
            with self.subTest(request=request):
                self.assertFalse(validator.is_valid(request))


if __name__ == "__main__":
    unittest.main()
