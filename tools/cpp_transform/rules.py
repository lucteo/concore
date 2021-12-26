import sys
import yaml
from cpp_transform.IncludeToQuotes import IncludeToQuotes
from cpp_transform.NamespaceRename import NamespaceRename

_all_rules = {
    "IncludeToQuotes": IncludeToQuotes,
    "NamespaceRename": NamespaceRename,
}


def load_rules(rules_file):
    """Load the rules from the given .transform-rules file"""
    # Read the rules content
    with open(rules_file) as f:
        rules_content = yaml.safe_load(f)

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
