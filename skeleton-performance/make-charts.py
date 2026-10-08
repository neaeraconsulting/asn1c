#!/usr/bin/env python3
"""Writes the two bar charts of SKELETON-PERFORMANCE.md, each with a panel per direction:
cpu-per-conversion.svg and heap-allocations.svg.

CPU holds user-mode CPU cycles per convert_bytes call (perf stat, min of 3), for the
original skeletons, the updated ones, and the updated ones with the arena. The chart
compares the first with the last: everything, arena included.

ALLOCATIONS holds the heap allocations made by one convert_bytes call, for the
original skeletons and for the updated ones with the arena. The latter is 0,
or 1 for the largest messages, so its bar is a sliver with the count above it.

Edit CPU or ALLOCATIONS and run this script from any directory to redraw the charts.
"""
import os

# direction: [(message, original, updated, updated with arena)]
CPU = {
    "UPER to JER": [("BSM", 14166, 8750, 7546), ("PSM", 37979, 24758, 20971),
                    ("SPAT", 411275, 271235, 211667), ("MAP", 684715, 468327, 321488)],
    "UPER to XER": [("BSM", 15205, 9975, 8831), ("PSM", 38873, 26803, 23198),
                    ("SPAT", 439258, 307447, 251664), ("MAP", 723850, 524245, 369911)],
    "JER to UPER": [("BSM", 22722, 16589, 15527), ("PSM", 62441, 46651, 41649),
                    ("SPAT", 637839, 472680, 412428), ("MAP", 951867, 702097, 617022)],
    "XER to UPER": [("BSM", 23477, 19751, 17016), ("PSM", 64936, 55321, 46221),
                    ("SPAT", 692054, 581429, 480832), ("MAP", 1040458, 853828, 698355)],
}
# direction: [(message, original, updated with arena)]
ALLOCATIONS = {
    "UPER to JER": [("BSM", 47, 0), ("PSM", 154, 0), ("SPAT", 1968, 0), ("MAP", 2710, 1)],
    "UPER to XER": [("BSM", 47, 0), ("PSM", 154, 0), ("SPAT", 1968, 0), ("MAP", 2710, 1)],
    "JER to UPER": [("BSM", 65, 0), ("PSM", 203, 0), ("SPAT", 2262, 0), ("MAP", 3107, 1)],
    "XER to UPER": [("BSM", 72, 0), ("PSM", 212, 0), ("SPAT", 2429, 0), ("MAP", 3269, 1)],
}
SERIES = [("Original skeletons", "#2a78d6"), ("Updated skeletons", "#eb6834")]
SURFACE, INK, INK2, MUTED, GRID, AXIS = "#fcfcfb", "#0b0b0b", "#52514e", "#898781", "#e1e0d9", "#c3c2b7"
FONT = "system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif"

W, H = 760, 590
LEFT, TOP, PANEL_W, PANEL_H, COL_GAP, ROW_GAP = 56, 132, 318, 150, 44, 96
BAR, GAP = 26, 2


def cycles(n):
    return f"{n / 1e6:.2f}M" if n >= 1e6 else f"{n / 1e3:.0f}k" if n >= 1e5 else f"{n / 1e3:.1f}k"


def bar(x, y, w, h, fill):
    """A bar with its top corners rounded, standing on the baseline."""
    r = min(4, h)
    return (f'<path d="M{x},{y + h} V{y + r} Q{x},{y} {x + r},{y} H{x + w - r} '
            f'Q{x + w},{y} {x + w},{y + r} V{y + h} Z" fill="{fill}"/>')


def chart(title, subtitle, panels, min_height, bar_label, group_label):
    """Small multiples: a panel per direction, the messages side by side in each.

    panels: {direction: [(message, original, updated)]}. The bars are drawn as a share of
    the original. min_height(updated) is the least height of the second bar, for values
    too small to see. bar_label and group_label give the text above it and under the pair.
    """
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
           f'font-family="{FONT}" role="img" aria-labelledby="t d">',
           f'<title id="t">{title}</title>',
           '<desc id="d">' + "; ".join(
               f"{name}: " + ", ".join(f"{m} {group_label(o, u)}" for m, o, u in rows)
               for name, rows in panels.items()) + '</desc>',
           f'<rect width="{W}" height="{H}" fill="{SURFACE}"/>',
           f'<text x="16" y="30" font-size="17" font-weight="600" fill="{INK}">{title}</text>',
           f'<text x="16" y="50" font-size="12.5" fill="{INK2}">{subtitle}</text>']
    x = 16
    for name, color in SERIES:
        out.append(f'<rect x="{x}" y="64" width="12" height="12" rx="3" fill="{color}"/>')
        out.append(f'<text x="{x + 18}" y="74.5" font-size="12.5" fill="{INK2}">{name}</text>')
        x += 18 + 6.5 * len(name) + 24
    for p, (name, rows) in enumerate(panels.items()):
        px = LEFT + (p % 2) * (PANEL_W + COL_GAP)
        base = TOP + (p // 2) * (PANEL_H + ROW_GAP) + PANEL_H
        out.append(f'<text x="{px - 40}" y="{base - PANEL_H - 22}" font-size="13.5" font-weight="600" '
                   f'fill="{INK}">{name}</text>')
        for pct in (0, 50, 100):
            y = base - PANEL_H * pct / 100
            out.append(f'<line x1="{px}" x2="{px + PANEL_W}" y1="{y}" y2="{y}" '
                       f'stroke="{AXIS if pct == 0 else GRID}"/>')
            out.append(f'<text x="{px - 8}" y="{y + 4}" font-size="11.5" fill="{MUTED}" '
                       f'text-anchor="end">{pct}%</text>')
        group = PANEL_W / len(rows)
        for i, (msg, orig, upd) in enumerate(rows):
            cx = px + group * (i + 0.5)
            x0 = cx - (2 * BAR + GAP) / 2
            for j, value in enumerate((orig, upd)):
                h = max(PANEL_H * value / orig, min_height(value) if j else 0)
                bx = x0 + j * (BAR + GAP)
                out.append(bar(round(bx, 1), round(base - h, 1), BAR, round(h, 1), SERIES[j][1]))
                if j:
                    out.append(f'<text x="{bx + BAR / 2}" y="{base - h - 6:.1f}" font-size="12" '
                               f'font-weight="600" fill="{INK}" text-anchor="middle">'
                               f'{bar_label(orig, upd)}</text>')
            out.append(f'<text x="{cx}" y="{base + 19}" font-size="12.5" font-weight="600" fill="{INK}" '
                       f'text-anchor="middle">{msg}</text>')
            out.append(f'<text x="{cx}" y="{base + 35}" font-size="11.5" fill="{MUTED}" '
                       f'text-anchor="middle">{group_label(orig, upd)}</text>')
    out.append('</svg>')
    return "\n".join(out) + "\n"


CHARTS = {
    "cpu-per-conversion.svg": chart(
        "CPU per conversion",
        "CPU cycles per call, as a share of the original skeletons, with every update and the arena. "
        "Lower is better.",
        {name: [(m, o, a) for m, o, _, a in rows] for name, rows in CPU.items()},
        lambda value: 0,
        lambda orig, upd: f"{upd / orig:.0%}",
        lambda orig, upd: f"{cycles(orig)} → {cycles(upd)}"),
    "heap-allocations.svg": chart(
        "Heap allocations per conversion",
        "Allocations per call, as a share of the original skeletons, with every update and the arena. "
        "Lower is better.",
        ALLOCATIONS,
        # 0 and 1 in thousands are too thin to see: a 2px sliver for 0 keeps the
        # series visible, and 1 gets 4px to stand apart from it.
        lambda value: 4 if value else 2,
        lambda orig, upd: f"{upd}",
        lambda orig, upd: f"{orig} → {upd}"),
}

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    for name, svg in CHARTS.items():
        path = os.path.join(here, name)
        with open(path, "w", encoding="utf-8") as f:
            f.write(svg)
        print(path)
