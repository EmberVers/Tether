"""Material master-template generators (M3).

Each submodule exposes a ``build(...)`` entry point that assembles one
AAA-aligned master material with sensible parameter defaults, optional
static-switch feature flags, a compile pass, and (optionally) a test
Material Instance + preview render.

Invocation is always via the tether from the host side::

    python tether.py exec --stdin <<'EOF'
    import sys
    sys.path.insert(0, r"<project>/Plugins/Tether/Content/Python")
    from material_templates import character_armor
    r = character_armor.build()
    print(r)
    EOF

All template builders assume they run inside the UE editor Python
environment — they import ``unreal`` directly and call the
``TetherMaterialLibrary`` M2/M2.5 primitives.
"""
