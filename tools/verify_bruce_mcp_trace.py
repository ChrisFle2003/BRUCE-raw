#!/usr/bin/env python3
"""Read-only command-line verifier for one local BRUCE MCP trace."""
import sys

from bruce_mcp_trace import canonical_json, verify_trace


def main(argv=None):
    arguments = sys.argv[1:] if argv is None else argv
    if len(arguments) != 1:
        result = {"status": "INVALID_FORMAT", "trace_version": None, "run_id": None,
                  "message_count": None, "final_hash": None, "reason": "InvalidArguments"}
    else:
        result = verify_trace(arguments[0])
    print(canonical_json(result))
    return 0 if result["status"] == "VALID" else 1


if __name__ == "__main__":
    sys.exit(main())
