from __future__ import annotations

import re
from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor
from docx.enum.text import WD_BREAK


ROOT = Path(r"F:\workspace\N SCAL SPEEDTEST_RAW\DOC")
SOURCE = ROOT / "NSCAL_SPEEDTEST_RAW_Design.md"
OUTPUT = ROOT / "NSCAL_SPEEDTEST_RAW_Design.docx"

FONT_CN = "微软雅黑"
FONT_CODE = "Consolas"


def set_run_font(run, name=FONT_CN, size=None, bold=None, color=None):
    run.font.name = name
    run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), name)
    if size is not None:
        run.font.size = Pt(size)
    if bold is not None:
        run.bold = bold
    if color is not None:
        run.font.color.rgb = color


def shade_cell(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), fill)
    tc_pr.append(shd)


def set_cell_margins(cell, top=60, bottom=60, left=90, right=90):
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for tag, value in (("top", top), ("bottom", bottom), ("left", left), ("right", right)):
        node = tc_mar.find(qn(f"w:{tag}"))
        if node is None:
            node = OxmlElement(f"w:{tag}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def add_page_number(paragraph):
    run = paragraph.add_run()
    fld_char1 = OxmlElement("w:fldChar")
    fld_char1.set(qn("w:fldCharType"), "begin")
    instr_text = OxmlElement("w:instrText")
    instr_text.set(qn("xml:space"), "preserve")
    instr_text.text = " PAGE "
    fld_char2 = OxmlElement("w:fldChar")
    fld_char2.set(qn("w:fldCharType"), "end")
    run._r.append(fld_char1)
    run._r.append(instr_text)
    run._r.append(fld_char2)


def add_inline(paragraph, text, size=10.5, base_bold=False):
    parts = re.split(r"(`[^`]*`|\*\*[^*]+\*\*)", text)
    for part in parts:
        if not part:
            continue
        if part.startswith("`") and part.endswith("`") and len(part) >= 2:
            run = paragraph.add_run(part[1:-1])
            set_run_font(run, FONT_CODE, size - 0.7)
        elif part.startswith("**") and part.endswith("**") and len(part) >= 4:
            run = paragraph.add_run(part[2:-2].replace("`", ""))
            set_run_font(run, FONT_CN, size, bold=True)
        else:
            run = paragraph.add_run(part)
            set_run_font(run, FONT_CN, size, bold=base_bold)


def add_code_block(document, lines):
    paragraph = document.add_paragraph()
    paragraph.style = document.styles["No Spacing"]
    paragraph.paragraph_format.space_before = Pt(3)
    paragraph.paragraph_format.space_after = Pt(5)
    paragraph.paragraph_format.left_indent = Cm(0.35)
    paragraph.paragraph_format.line_spacing = 1.0
    p_pr = paragraph._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), "F2F2F2")
    p_pr.append(shd)
    for i, line in enumerate(lines):
        run = paragraph.add_run(line)
        set_run_font(run, FONT_CODE, 8.3)
        if i < len(lines) - 1:
            run.add_break(WD_BREAK.LINE)


def add_markdown_table(document, table_lines):
    rows = []
    for line in table_lines:
        stripped = line.strip()
        if not stripped.startswith("|"):
            continue
        cells = [c.strip() for c in stripped.strip("|").split("|")]
        if all(re.fullmatch(r":?-{3,}:?", c) for c in cells if c):
            continue
        rows.append(cells)
    if not rows:
        return
    cols = max(len(r) for r in rows)
    table = document.add_table(rows=len(rows), cols=cols)
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = True
    for r_i, row in enumerate(rows):
        tr = table.rows[r_i]._tr
        trPr = tr.get_or_add_trPr()
        cant_split = OxmlElement("w:cantSplit")
        trPr.append(cant_split)
        if r_i == 0:
            tbl_header = OxmlElement("w:tblHeader")
            trPr.append(tbl_header)
        for c_i in range(cols):
            cell = table.cell(r_i, c_i)
            set_cell_margins(cell)
            cell.text = ""
            p = cell.paragraphs[0]
            p.paragraph_format.space_before = Pt(0)
            p.paragraph_format.space_after = Pt(0)
            value = row[c_i] if c_i < len(row) else ""
            add_inline(p, value, size=9.0, base_bold=(r_i == 0))
            if r_i == 0:
                shade_cell(cell, "D9E2F3")
    document.add_paragraph().paragraph_format.space_after = Pt(0)


def add_toc(document, headings):
    heading = document.add_paragraph()
    heading.paragraph_format.space_before = Pt(10)
    heading.paragraph_format.space_after = Pt(4)
    run = heading.add_run("目录")
    set_run_font(run, FONT_CN, 16, bold=True, color=RGBColor(0x1F, 0x4E, 0x79))

    for entry in headings:
        p = document.add_paragraph()
        p.paragraph_format.left_indent = Cm(0.6)
        p.paragraph_format.space_after = Pt(2)
        r = p.add_run(entry)
        set_run_font(r, FONT_CN, 10.5)


def build():
    lines = SOURCE.read_text(encoding="utf-8").splitlines()

    document = Document()
    section = document.sections[0]
    section.page_width = Cm(21.0)
    section.page_height = Cm(29.7)
    section.top_margin = Cm(2.0)
    section.bottom_margin = Cm(2.0)
    section.left_margin = Cm(2.1)
    section.right_margin = Cm(2.1)

    normal = document.styles["Normal"]
    normal.font.name = FONT_CN
    normal._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), FONT_CN)
    normal.font.size = Pt(10.5)
    normal.paragraph_format.space_after = Pt(4)
    normal.paragraph_format.line_spacing = 1.25

    for style_name, size, color in (
        ("Heading 1", 16, RGBColor(0x1F, 0x4E, 0x79)),
        ("Heading 2", 13, RGBColor(0x2E, 0x74, 0xB5)),
        ("Heading 3", 11, RGBColor(0x33, 0x33, 0x33)),
    ):
        style = document.styles[style_name]
        style.font.name = FONT_CN
        style._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), FONT_CN)
        style.font.size = Pt(size)
        style.font.color.rgb = color
        style.font.bold = True
        style.paragraph_format.keep_with_next = True
        style.paragraph_format.space_before = Pt(10)
        style.paragraph_format.space_after = Pt(4)

    # Header / footer
    header = section.header
    hp = header.paragraphs[0]
    hp.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    hr = hp.add_run("N SCAL SPEEDTEST RAW  |  设计文档")
    set_run_font(hr, FONT_CN, 8.5, color=RGBColor(0x66, 0x66, 0x66))

    footer = section.footer
    fp = footer.paragraphs[0]
    fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    fr = fp.add_run("第 ")
    set_run_font(fr, FONT_CN, 8.5)
    add_page_number(fp)
    fr2 = fp.add_run(" 页")
    set_run_font(fr2, FONT_CN, 8.5)

    # Cover
    title = document.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    title.paragraph_format.space_before = Pt(120)
    tr = title.add_run("N SCAL SPEEDTEST RAW")
    set_run_font(tr, FONT_CN, 26, bold=True, color=RGBColor(0x1F, 0x4E, 0x79))

    subtitle = document.add_paragraph()
    subtitle.alignment = WD_ALIGN_PARAGRAPH.CENTER
    sr = subtitle.add_run("列车测速系统设计文档（RAW 版）")
    set_run_font(sr, FONT_CN, 18, bold=True)

    meta = document.add_paragraph()
    meta.alignment = WD_ALIGN_PARAGRAPH.CENTER
    meta.paragraph_format.space_before = Pt(22)
    mr = meta.add_run("V1.0  |  2026-11-07")
    set_run_font(mr, FONT_CN, 11, color=RGBColor(0x66, 0x66, 0x66))

    note = document.add_paragraph()
    note.alignment = WD_ALIGN_PARAGRAPH.CENTER
    note.paragraph_format.space_before = Pt(10)
    nr = note.add_run("STM32F103C8T6 / HK32F103C8T6 · Keil MDK 5 · Arm Compiler 5")
    set_run_font(nr, FONT_CN, 9, color=RGBColor(0x66, 0x66, 0x66))

    document.add_page_break()

    # Parse markdown
    i = 0
    paragraph_buffer = []
    first_section_seen = False
    toc_headings = [re.sub(r"^##\s+", "", line).strip()
                    for line in lines if re.match(r"^##\s+", line)]
    toc_headings = [h for h in toc_headings if h and not h.startswith("文档信息")]

    def flush_paragraph():
        nonlocal paragraph_buffer
        if not paragraph_buffer:
            return
        text = " ".join(s.strip() for s in paragraph_buffer if s.strip())
        paragraph_buffer = []
        if not text:
            return
        p = document.add_paragraph()
        add_inline(p, text, size=10.5)

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if stripped.startswith("```"):
            flush_paragraph()
            i += 1
            code_lines = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code_lines.append(lines[i])
                i += 1
            add_code_block(document, code_lines)
            i += 1
            continue

        if stripped.startswith("|"):
            flush_paragraph()
            table_lines = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                table_lines.append(lines[i])
                i += 1
            add_markdown_table(document, table_lines)
            continue

        if stripped.startswith("#"):
            flush_paragraph()
            m = re.match(r"^(#{1,4})\s+(.*)$", stripped)
            if not m:
                i += 1
                continue
            level = len(m.group(1))
            text = m.group(2).strip()
            if level == 1:
                # 封面已经包含标题；跳过 Markdown 的一级标题，避免重复标题页。
                i += 1
                continue
            else:
                if level == 2 and text.startswith("1.") and not first_section_seen:
                    first_section_seen = True
                    add_toc(document, toc_headings)
                    document.add_page_break()
                elif level == 2 and text.startswith("2."):
                    document.add_page_break()
                document.add_heading(text, level=max(1, level - 1))
            i += 1
            continue

        if re.match(r"^\s*[-*]\s+", line):
            flush_paragraph()
            p = document.add_paragraph(style="List Bullet")
            add_inline(p, re.sub(r"^\s*[-*]\s+", "", line), size=10.5)
            i += 1
            continue

        if re.match(r"^\s*\d+\.\s+", line):
            flush_paragraph()
            p = document.add_paragraph()
            p.paragraph_format.left_indent = Cm(0.6)
            p.paragraph_format.first_line_indent = Cm(-0.6)
            add_inline(p, line.strip(), size=10.5)
            i += 1
            continue

        if not stripped:
            flush_paragraph()
            i += 1
            continue

        paragraph_buffer.append(line)
        i += 1

    flush_paragraph()

    # Document properties
    document.core_properties.title = "N SCAL SPEEDTEST RAW 列车测速系统设计文档"
    document.core_properties.subject = "RAW 版测速、消抖、N 比例换算与通信设计"
    document.core_properties.author = "N SCAL SPEEDTEST RAW 项目"
    document.core_properties.keywords = "STM32F103C8T6, TIM3, TIM4, Nextion, RAW speed, N scale"

    document.save(OUTPUT)
    print(f"saved: {OUTPUT}")
    print(f"paragraphs={len(document.paragraphs)} tables={len(document.tables)}")


if __name__ == "__main__":
    build()
