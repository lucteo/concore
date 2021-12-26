import sys
import yaml
from jsonschema import validate
from cpp_transform.IncludeToQuotes import IncludeToQuotes
from cpp_transform.NamespaceRename import NamespaceRename
from cpp_transform.TokenIdReplace import TokenIdReplace

_all_rules = {
    "IncludeToQuotes": IncludeToQuotes,
    "NamespaceRename": NamespaceRename,
    "TokenIdReplace": TokenIdReplace,
}

_schema = """
type: array
items:
  type: object
  properties:
    IncludeToQuotes:
      type: array
      items:
        type: string
    NamespaceRename:
      type: object
      properties:
        from:
          type: string
        to:
          type: string
      required:
      - from
      - to
    TokenIdReplace:
      type: object
      properties:
        from:
          type: string
        to:
          type: string
        prev:
          type: array
          default: []
          items:
            type: object
            properties:
              token:
                enum:
                - punct
                - id
              text:
                type: string
            required:
            - token
            - text
        after:
          type: array
          default: []
          items:
            type: object
            properties:
              token:
                enum:
                - punct
                - id
              text:
                type: string
            required:
            - token
            - text
      required:
      - from
      - to
      optional:
      - prev
      - after
"""


def load_rules(rules_file):
    """Load the rules from the given .transform-rules file"""
    # Read the rules content
    with open(rules_file) as f:
        rules_content = yaml.safe_load(f)

    # Validate the rules content
    validate(rules_content, yaml.safe_load(_schema))

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
