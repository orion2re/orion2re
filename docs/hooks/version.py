from pathlib import Path


VERSION_FILE = Path(__file__).resolve().parents[2] / "VERSION"


def on_page_markdown(markdown, **kwargs):
    return markdown.replace(
        "{{ engine_version }}", VERSION_FILE.read_text(encoding="utf-8").strip()
    )
