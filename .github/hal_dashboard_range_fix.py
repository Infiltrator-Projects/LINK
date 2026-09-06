from pathlib import Path

p = Path('src/core/dashboard.c')
text = p.read_text()
old = '''bool link_dashboard_gauge_range_for_parameter(\n    const LinkParameterDefinition *definition,\n    LinkDashboardGaugeRange *range)\n{\n    if (definition == NULL || range == NULL ||\n        !link_parameter_definition_is_valid(definition) ||\n        !isfinite(definition->minimum) || !isfinite(definition->maximum) ||\n        definition->maximum <= definition->minimum) {\n        return false;\n    }\n    range->minimum = definition->minimum;\n    range->maximum = definition->maximum;\n    return true;\n}\n'''
new = '''bool link_dashboard_gauge_range_for_parameter(\n    const LinkParameterDefinition *definition,\n    LinkDashboardGaugeRange *range)\n{\n    /*\n     * Dashboard scale is presentation metadata, not a decoder clamp. The\n     * five scales below are the established Linux cockpit scales and are now\n     * centralised here so every platform draws the same instrument.\n     */\n    static const struct {\n        const char *stable_key;\n        double minimum;\n        double maximum;\n    } reference_ranges[] = {\n        { "obd2.engine.rpm", 0.0, 7000.0 },\n        { "obd2.vehicle.speed", 0.0, 260.0 },\n        { "obd2.engine.coolant", -40.0, 150.0 },\n        { "obd2.diesel.rail_pressure", 0.0, 200000.0 },\n        { "obd2.fuel.tank_level", 0.0, 100.0 }\n    };\n    size_t index;\n\n    if (definition == NULL || range == NULL || definition->stable_key == NULL)\n        return false;\n\n    for (index = 0U; index < sizeof(reference_ranges) / sizeof(reference_ranges[0]); ++index) {\n        if (strcmp(definition->stable_key, reference_ranges[index].stable_key) == 0) {\n            range->minimum = reference_ranges[index].minimum;\n            range->maximum = reference_ranges[index].maximum;\n            return true;\n        }\n    }\n\n    /*\n     * Later SAE definitions already carry meaningful finite protocol ranges.\n     * Reuse those when available; 0..0 means no dashboard scale is declared.\n     */\n    if (!isfinite(definition->minimum) || !isfinite(definition->maximum) ||\n        definition->maximum <= definition->minimum) {\n        return false;\n    }\n    range->minimum = definition->minimum;\n    range->maximum = definition->maximum;\n    return true;\n}\n'''
if old not in text:
    raise SystemExit('dashboard range function anchor not found')
p.write_text(text.replace(old, new, 1))

p = Path('tests/test_dashboard.c')
text = p.read_text()
old = '''    CHECK(link_dashboard_gauge_range_for_parameter(rpm, &range));\n    CHECK(range.maximum > range.minimum);\n'''
new = '''    CHECK(link_dashboard_gauge_range_for_parameter(rpm, &range));\n    CHECK(fabs(range.minimum - 0.0) < 1e-12);\n    CHECK(fabs(range.maximum - 7000.0) < 1e-12);\n'''
if old not in text:
    raise SystemExit('dashboard test range anchor not found')
p.write_text(text.replace(old, new, 1))

Path('.github/hal_dashboard_range_fix.py').unlink()
