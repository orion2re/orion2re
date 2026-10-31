import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("audit_rpc_contract", ROOT / "tools/audit_rpc_contract.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class RegistryTests(unittest.TestCase):
    def setUp(self):
        self.registry = audit.Load_Json(ROOT / "specs/rpc/methods.json")
        self.plan = (ROOT / "plan.md").read_text(encoding="utf-8")
        self.expected = audit.Catalog_Methods(self.plan)

    def Validate(self):
        return audit.Validate_Methods(self.registry, self.expected)

    def test_full_catalog_remains_disabled(self):
        self.assertEqual(self.Validate(), 60)
        self.assertIn("game.getState", self.expected)
        self.assertNotIn("game.getState(include)", self.expected)
        self.assertNotIn("harold.cpp", self.expected)

    def test_duplicate_method_is_rejected(self):
        self.registry["methods"].append(copy.deepcopy(self.registry["methods"][0]))
        with self.assertRaisesRegex(ValueError, "duplicate method"):
            self.Validate()

    def test_missing_and_extra_methods_are_rejected(self):
        for name in (None, "game.getMemory"):
            with self.subTest(name=name):
                registry = copy.deepcopy(self.registry)
                if name is None:
                    registry["methods"].pop()
                else:
                    registry["methods"][-1]["name"] = name
                with self.assertRaisesRegex(ValueError, "catalog mismatch"):
                    audit.Validate_Methods(registry, self.expected)

    def test_extra_descriptor_property_is_rejected(self):
        for key, value in (("contract", {}), ("enabled", True)):
            with self.subTest(key=key):
                registry = copy.deepcopy(self.registry)
                registry["methods"][0][key] = value
                with self.assertRaises(ValueError):
                    audit.Validate_Methods(registry, self.expected)

    def test_registry_cannot_enable_or_change_perspective(self):
        for key, value in (("status", "enabled"), ("perspective", "omniscient"),
                           ("unknownFields", "serialize")):
            with self.subTest(key=key):
                registry = copy.deepcopy(self.registry)
                registry[key] = value
                with self.assertRaises(ValueError):
                    audit.Validate_Methods(registry, self.expected)

    def test_action_requires_controller(self):
        action = next(item for item in self.registry["methods"] if item["kind"] == "action")
        action["role"] = "reader"
        with self.assertRaisesRegex(ValueError, "requires controller"):
            self.Validate()

    def test_only_hello_can_be_unauthenticated(self):
        self.registry["methods"][1]["role"] = "unauthenticated"
        with self.assertRaisesRegex(ValueError, "only rpc.hello"):
            self.Validate()

    def test_action_cannot_disguise_itself_as_query(self):
        action = next(item for item in self.registry["methods"] if item["kind"] == "action")
        action.update(kind="query", role="reader")
        with self.assertRaisesRegex(ValueError, "method kind disagrees"):
            self.Validate()

    def test_discovery_does_not_require_controller(self):
        self.registry["methods"][1]["role"] = "controller"
        with self.assertRaisesRegex(ValueError, "requires reader role"):
            self.Validate()

    def test_invalid_descriptor_enums_are_rejected(self):
        for key in ("owner", "role", "kind", "availability"):
            with self.subTest(key=key):
                registry = copy.deepcopy(self.registry)
                registry["methods"][0][key] = "invalid"
                with self.assertRaises(ValueError):
                    audit.Validate_Methods(registry, self.expected)


class LedgerTests(unittest.TestCase):
    def setUp(self):
        self.commit = "12345678" + "0" * 32
        self.plan = (
            "## Appendix F. Runtime\n"
            "| Commit(s) | Change | Acceptance |\n| --- | --- | --- |\n"
            "| 12345678 | Feature | Native parity |\n"
            "## Appendix G. Documentation\n"
            "| Commit | Disposition |\n| --- | --- |\n"
            "## Appendix H. Changed-file coverage and robustness findings\n"
            "| Changed files | Disposition / owner |\n| --- | --- |\n"
            "| `src/game/example.cpp` | Scoped adapter and lifetime tests |\n"
            "### Fork weaknesses\n`not-a-changed-file`\n")

    def test_one_entry_per_full_commit_and_file_without_signoff(self):
        commits = audit.Commit_Ledger(self.plan, [self.commit])
        files = audit.File_Ledger(self.plan, ["src/game/example.cpp"])
        self.assertEqual(commits[self.commit]["acceptance"], "Native parity")
        for entry in (commits[self.commit], files["src/game/example.cpp"]):
            self.assertEqual(entry["status"], "planned")
            self.assertIsNone(entry["implementation"])
            self.assertEqual(entry["tests"], [])

    def test_same_counts_wrong_identity_fails(self):
        with self.assertRaises(ValueError):
            audit.Commit_Ledger(self.plan, ["abcdef12" + "0" * 32])
        with self.assertRaisesRegex(ValueError, "coverage mismatch"):
            audit.File_Ledger(self.plan, ["src/game/different.cpp"])

    def test_duplicate_commit_or_path_fails(self):
        with self.assertRaisesRegex(ValueError, "duplicate fork commit"):
            audit.Commit_Ledger(self.plan.replace("## Appendix G.",
                "| 12345678 | Duplicate | Duplicate |\n## Appendix G."), [self.commit])
        with self.assertRaisesRegex(ValueError, "duplicate changed file"):
            audit.File_Ledger(self.plan.replace("### Fork weaknesses",
                "| `src/game/example.cpp` | Duplicate |\n### Fork weaknesses"),
                ["src/game/example.cpp"])

    def test_uncovered_commit_fails(self):
        with self.assertRaisesRegex(ValueError, "coverage mismatch"):
            audit.Commit_Ledger(self.plan, [self.commit, "abcdef12" + "0" * 32])

    def test_grouped_commit_row_expands_without_dropping_amendment(self):
        other = "abcdef12" + "0" * 32
        plan = self.plan.replace("| 12345678 |", "| 12345678, abcdef12 |")
        ledger = audit.Commit_Ledger(plan, [self.commit, other])
        self.assertEqual(set(ledger), {self.commit, other})
        self.assertEqual(ledger[self.commit], ledger[other])

    def test_duplicate_json_member_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "duplicate.json"
            path.write_text('{"name":1,"na\\u006de":2}', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate JSON member"):
                audit.Load_Json(path)

    def test_target_drift_excludes_only_p0_artifacts(self):
        audit.Validate_Target_Drift(["plan.md", "tools/audit_rpc_contract.py",
                                     "specs/rpc/methods.json", "tests/rpc/test_contract.py"])
        for path in ("src/game/harold.cpp", "specs/rpc_other/file", "src/game/combat1.cpp"):
            with self.subTest(path=path), self.assertRaisesRegex(ValueError, "target drift"):
                audit.Validate_Target_Drift([path])

    def test_full_git_inventory(self):
        report = audit.Audit(ROOT)
        self.assertEqual(len(report["commits"]), 119)
        self.assertEqual(len(report["files"]), 61)
        self.assertEqual(sum(entry["category"] == "runtime"
                             for entry in report["commits"].values()), 92)
        self.assertEqual(report, json.loads(json.dumps(report)))


class TypedContractTests(unittest.TestCase):
    def setUp(self):
        self.contracts = audit.Load_Json(ROOT / "specs/rpc/contracts.json")
        self.policies = audit.Load_Json(ROOT / "specs/rpc/field_policies.json")
        self.registry = audit.Load_Json(ROOT / "specs/rpc/methods.json")

    def Sample(self, name):
        schema = self.contracts["types"][name]
        kind = schema["kind"]
        if kind == "object":
            return {key: self.Sample(value) for key, value in schema["fields"].items()}
        if kind == "array":
            return [self.Sample(schema["items"])] if schema.get("minItems", 0) else []
        if kind == "union":
            return self.Sample(schema["variants"][0])
        if kind == "enum":
            return schema["values"][0]
        if kind == "string":
            return "0" * schema["minBytes"] if "pattern" in schema else "x" * schema["minBytes"]
        if kind == "integer":
            return max(0, schema["min"])
        return True

    def test_all_methods_have_typed_request_and_result_fixtures(self):
        for name, (request, result) in self.contracts["methods"].items():
            with self.subTest(method=name):
                audit.Validate_Value(self.Sample(request), request, self.contracts)
                audit.Validate_Value(self.Sample(result), result, self.contracts)

    def test_contract_graph_and_all_native_evidence_anchors(self):
        count = audit.Validate_Contracts(ROOT, self.registry, self.contracts, self.policies)
        self.assertGreater(count, 100)

    def test_closed_objects_and_no_type_coercion(self):
        for value, name in (({"id":"x","viewerPlayerId":0}, "GetRequest"),
                             (True,"Count"), ("1","Count"), (1.0,"Count"),
                             (None,"Text"), ("000","Counter"),
                             ("18446744073709551616","Counter"), ([],"NonemptyIds")):
            with self.subTest(value=value, name=name), self.assertRaises(ValueError):
                audit.Validate_Value(value, name, self.contracts)

    def test_string_limits_are_utf8_bytes_not_characters(self):
        audit.Validate_Value("é" * 64, "Id", self.contracts)
        with self.assertRaises(ValueError):
            audit.Validate_Value("é" * 65, "Id", self.contracts)

    def test_known_empty_and_unknown_are_distinct(self):
        audit.Validate_Value({"availability":"current","value":""}, "KnownText", self.contracts)
        audit.Validate_Value({"availability":"unknown"}, "KnownText", self.contracts)
        for value in (None, "", {"availability":"unknown","value":"secret"}):
            with self.subTest(value=value), self.assertRaises(ValueError):
                audit.Validate_Value(value, "KnownText", self.contracts)

    def test_unaudited_object_and_field_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "disabled object"):
            audit.Validate_Value(self.Sample("Fleet"), "Fleet", self.contracts, self.policies, public=True)
        system = self.Sample("System")
        with self.assertRaisesRegex(ValueError, "disabled field"):
            audit.Validate_Value(system, "System", self.contracts, self.policies, public=True)
        system["owner"] = {"availability":"unaudited"}
        audit.Validate_Value(system, "System", self.contracts, self.policies, public=True)
        ship = self.Sample("Ship")
        with self.assertRaisesRegex(ValueError, "disabled field"):
            audit.Validate_Value(ship, "Ship", self.contracts, self.policies, public=True)
        for field in ("owner", "destination", "eta"):
            ship[field] = {"availability":"unaudited"}
        audit.Validate_Value(ship, "Ship", self.contracts, self.policies, public=True)

    def test_missing_policy_and_recursive_contract_are_rejected(self):
        broken = copy.deepcopy(self.contracts)
        broken["types"]["System"]["fieldPolicies"]["name"] = "no-such-policy"
        with self.assertRaisesRegex(ValueError, "missing field evidence"):
            audit.Validate_Contracts(ROOT, self.registry, broken, self.policies)
        broken = copy.deepcopy(self.contracts)
        broken["types"]["System"]["fields"]["self"] = "System"
        with self.assertRaisesRegex(ValueError, "recursive contract"):
            audit.Validate_Contracts(ROOT, self.registry, broken, self.policies)

    def test_describe_is_explicit_and_uses_the_same_contract_source(self):
        described = audit.Describe_Contracts(self.registry, self.contracts)
        self.assertFalse(described["runtimeEnabled"])
        self.assertEqual(len(described["methods"]), 60)
        for method in described["methods"]:
            self.assertEqual([method["requestType"],method["resultType"]],
                             self.contracts["methods"][method["name"]])
        for name, schema in self.contracts["types"].items():
            if schema["kind"] == "object":
                self.assertFalse({"schema","schemas","actions"} & schema["fields"].keys(), name)

    def test_protocol_conformance_vectors(self):
        cases = audit.Load_Json(ROOT / "specs/rpc/protocol_cases.json")["cases"]
        for case in cases:
            with self.subTest(case=case["name"]):
                self.assertEqual(audit.Check_Request(case["json"].encode(), self.contracts,
                                                     case.get("authenticated", True)), case["code"])

    def test_input_size_depth_and_invalid_utf8(self):
        self.assertEqual(audit.Check_Request(b" " * 65537, self.contracts), -32005)
        self.assertEqual(audit.Check_Request(b"[" * 33 + b"]" * 33, self.contracts), -32005)
        self.assertEqual(audit.Check_Request(b"\xff", self.contracts), -32700)

    def test_persistent_ledger_requires_real_completion_evidence(self):
        report = audit.Audit(ROOT)
        ledger = audit.Load_Json(ROOT / "specs/rpc/coverage.json")
        key = next(iter(report["commits"]))
        ledger["evidence"][key] = {"status":"implemented", "implementation":["claimed"],
                                    "tests":[], "visibilityEvidence":[]}
        with self.assertRaisesRegex(ValueError, "completion lacks"):
            audit.Apply_Coverage(ledger, report["commits"], report["files"],
                                 {f"T-{number:02}" for number in range(1,21)})

    def test_persistent_ledger_cannot_omit_or_duplicate_a_commit(self):
        report = audit.Audit(ROOT)
        for operation in ("omit","duplicate"):
            with self.subTest(operation=operation):
                ledger = audit.Load_Json(ROOT / "specs/rpc/coverage.json")
                commits = ledger["commitGroups"][0]["commits"]
                if operation == "omit":
                    commits.pop()
                else:
                    commits.append(commits[0])
                with self.assertRaises(ValueError):
                    audit.Apply_Coverage(ledger, report["commits"], report["files"],
                                         {f"T-{number:02}" for number in range(1,21)})

    def test_arbitrary_nonempty_strings_cannot_certify_completion(self):
        with self.assertRaises(ValueError):
            audit.Validate_Completion({"status":"implemented", "implementation":["claimed"],
                                       "tests":["not really run"], "visibilityEvidence":["trust me"]},
                                      ["T-01"], ROOT)

    def test_completion_reports_are_hashed_and_must_record_success(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "implementation.cpp"
            source.write_text("fixture implementation\n")
            report = root / "test.json"
            report.write_text(json.dumps({"id":"T-01", "command":"fixture-check", "result":"passed", "exitCode":0}))
            def Reference(path):
                return {"path":path.name, "sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
            test = {"id":"T-01", "artifact":Reference(report)}
            evidence = {"status":"implemented", "implementation":[Reference(source)],
                        "tests":[test], "visibilityEvidence":[test]}
            audit.Validate_Completion(evidence, ["T-01"], root)
            report.write_text(json.dumps({"id":"T-01", "command":"fixture-check", "result":"failed", "exitCode":1}))
            with self.assertRaisesRegex(ValueError, "digest mismatch"):
                audit.Validate_Completion(evidence, ["T-01"], root)
            test["artifact"] = Reference(report)
            with self.assertRaisesRegex(ValueError, "not a passing"):
                audit.Validate_Completion(evidence, ["T-01"], root)


if __name__ == "__main__":
    unittest.main()
