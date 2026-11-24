# SPDX-License-Identifier: GPL-2.0-or-later
"""Export a print PDF using an explicitly chosen CMYK output profile."""

import os
from pathlib import Path
import shutil
import subprocess

import inkex
from inkex.command import ProgramRunError, inkscape


def read_output_profile(path):
    try:
        with Path(path).open("rb") as profile:
            header = profile.read(128)
            size = int.from_bytes(header[:4], "big")
            if (len(header) != 128 or not 128 <= size <= 64 * 1024 * 1024
                    or header[12:20] != b"prtrCMYK" or header[36:40] != b"acsp"):
                raise inkex.AbortExtension("Escolha um perfil ICC de saída CMYK válido.")
            tables = profile.read(size - 128)
            if len(tables) != size - 128:
                raise inkex.AbortExtension("O perfil ICC está incompleto.")
            return header + tables
    except OSError as exc:
        raise inkex.AbortExtension("Não foi possível abrir o perfil ICC selecionado.") from exc


def ghostscript_executable():
    names = ("gswin64c", "gswin32c") if os.name == "nt" else ("gs",)
    for name in names:
        executable = shutil.which(name)
        if executable:
            return executable
    raise inkex.AbortExtension("Instale o Ghostscript 10 e adicione o executável ao PATH.")


def convert_pdf(source, output, profile, standard, dpi):
    executable = ghostscript_executable()

    # Both the filename and the PDF namespaces in this PostScript are constants.
    # The profile bytes are read by Ghostscript with explicit SAFER permission.
    pdfx_metadata = (
        "[ /GTS_PDFXVersion (PDF/X-3:2002) /DOCINFO pdfmark\n"
        if standard == "pdfx3" else ""
    )
    setup = profile.with_name("output-intent.ps")
    setup.write_text(
        "%!PS\n" + pdfx_metadata +
        "[ /Title (Inksvector) /Trapped /False /DOCINFO pdfmark\n"
        "[ /_objdef {PrintICC} /type /stream /OBJ pdfmark\n"
        "[ {PrintICC} << /N 4 >> /PUT pdfmark\n"
        "[ {PrintICC} (press.icc) (r) file /PUT pdfmark\n"
        "[ {Catalog} << /OutputIntents [ << /Type /OutputIntent /S /GTS_PDFX\n"
        "  /OutputConditionIdentifier (Custom) /OutputCondition (CMYK ICC)\n"
        "  /DestOutputProfile {PrintICC} >> ] >> /PUT pdfmark\n",
        encoding="ascii",
    )

    # Paths controlled by the extension are relative to its private temporary directory.
    args = [
        executable, "-P-", "-dSAFER", "-dBATCH", "-dNOPAUSE", "-dPDFSTOPONERROR",
        "-sDEVICE=pdfwrite", "-sProcessColorModel=DeviceCMYK",
        "-sColorConversionStrategy=CMYK",
        "-sOutputICCProfile=press.icc", "--permit-file-read=press.icc",
        "-dAutoRotatePages=/None", "-dDownsampleColorImages=false",
        "-dDownsampleGrayImages=false", f"-r{dpi}", f"-sOutputFile={output.name}",
    ]
    if standard == "pdfx3":
        args += ["-dPDFX=3", "-dPDFACompatibilityPolicy=2"]
    else:
        args += ["-dCompatibilityLevel=1.7"]

    base_env = {key: value for key, value in os.environ.items()
                if key.upper() not in {"GS_OPTIONS", "GS_LIB"}}
    result = subprocess.run(
        [*args, setup.name, source.name], cwd=profile.parent, env=base_env,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False,
    )
    if result.returncode or not output.is_file():
        details = result.stdout.decode(errors="replace")[-2000:]
        raise inkex.AbortExtension(f"Falha na conversão para PDF CMYK.\n{details}")
    with output.open("rb") as stream:
        if stream.read(5) != b"%PDF-":
            raise inkex.AbortExtension("O Ghostscript não gerou um PDF válido.")


class CmykPdf(inkex.OutputExtension):
    def add_arguments(self, pars):
        pars.add_argument("--profile", default="")
        pars.add_argument("--standard", choices=("pdf", "pdfx3"), default="pdf")
        pars.add_argument("--dpi", type=int, choices=range(72, 1201), default=300)

    def save(self, stream):
        profile_data = read_output_profile(self.options.profile)
        temp = Path(self.tempdir).resolve()
        profile = temp / "press.icc"
        profile.write_bytes(profile_data)
        intermediate = temp / "document.pdf"
        try:
            inkscape(self.options.input_file, export_filename=str(intermediate),
                     export_dpi=self.options.dpi)
        except ProgramRunError as exc:
            raise inkex.AbortExtension("O Inksvector não conseguiu preparar o PDF de origem.") from exc
        if not intermediate.is_file():
            raise inkex.AbortExtension("O PDF de origem não foi criado.")

        output = temp / "print.pdf"
        convert_pdf(intermediate, output, profile, self.options.standard, self.options.dpi)
        with output.open("rb") as pdf:
            shutil.copyfileobj(pdf, stream)


if __name__ == "__main__":
    CmykPdf().run()
