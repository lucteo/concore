import sys
import os
import yaml
from jsonschema import validate
from cpp_transform.IncludeToQuotes import IncludeToQuotes
from cpp_transform.NamespaceRename import NamespaceRename
from cpp_transform.TokenIdReplace import TokenIdReplace
from cpp_transform.AddInNamespace import AddInNamespace

_all_rules = {
    "IncludeToQuotes": IncludeToQuotes,
    "NamespaceRename": NamespaceRename,
    "TokenIdReplace": TokenIdReplace,
    "AddInNamespace": AddInNamespace,
}


def load_rules(rules_file):
    """Load the rules from the given .transform-rules file"""
    # Read the rules content
    with open(rules_file) as f:
        rules_content = yaml.safe_load(f)

    # Read the rules file
    cur_dir = os.path.dirname(os.path.realpath(__file__))
    with open(cur_dir + "/rules_schema.yaml") as f:
        schema = yaml.safe_load(f)

    # Validate the rules content
    validate(rules_content, schema)

    # Generate the resulting list of rules objects
    res = []
    for rule in rules_content:
        for rule_name, params in rule.items():
            if rule_name in _all_rules:
                cls = _all_rules[rule_name]
                if cls:
                    rule = cls(params)
                    res.append(rule)
            else:
                print(f"ERROR: Unknown rule '{rule_name}'", file=sys.stderr)

    return res
