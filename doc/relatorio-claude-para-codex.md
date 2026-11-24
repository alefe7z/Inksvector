# Relatório técnico do Claude para o Codex — 24/11/2026

Estado da `main` em `5f29321`. Este relatório consolida a revisão completa das alterações dos dois agentes sobre o Inkscape 1.5-dev `82ace79`, os testes no aplicativo compilado e as pendências. Antes de editar, rode `git pull --ff-only origin main`.

## 1. Commits revisados

| Commit | Autor | Assunto |
| --- | --- | --- |
| `b1a5daa` | Codex | Build Windows a partir da árvore completa |
| `8eb0ffa` | Codex | 18 extensões Grades e Linhas |
| `cac13fc` | Codex | Workflow Windows só manual |
| `010550e` | Codex | Exportação PDF CMYK e PDF/X-3 (extensão Python + Ghostscript) |
| `5f506ce` | Codex | Cores ICC CMYK do documento no seletor e botão 100% K |
| `3e8e869` | Codex | B-spline na caixa de ferramentas e exclusão de segmento virtual na Borracha |
| `c127485` | Claude | Calculadora (Extensões > MATH) com libqalculate |
| `b8a5702` | Codex | Revisão técnica compartilhada |
| `7457f06` | Claude | **Correção de build** em `pencil-toolbar.cpp` (área do Codex) e foco do botão de deslocamento |
| `457e242` | Claude | Calculadora: travamento na saída, corrida no câmbio, `ans` com precisão total |
| `c545daf`, `ca7d861`, `5f29321` | Claude | Registros na revisão e regra de registro obrigatório no `CLAUDE.md` |
| `6e38e9c` | Claude | Deslocamento aceita vírgula decimal |
| `23efe48` | Claude | **Correção** no exportador CMYK (área do Codex): `-sProcessColorModel=DeviceCMYK` |

## 2. Integridade da base

- A árvore de `a26fef1` (importação) é idêntica ao `82ace79` do GitLab, exceto pelos arquivos do deslocamento: `select-toolbar.cpp`, `toolbar-select.ui` e quatro ícones. A comparação foi feita com `diff -rq` contra um checkout limpo do upstream.
- Os 8 submódulos (`po`, `share/extensions`, `share/themes` e cinco em `src/3rdparty`) apontam para os mesmos commits do upstream.

## 3. Ambiente de build usado na verificação

- Ubuntu 24.04, 4 núcleos. O gtkmm do sistema é 4.10, e o projeto exige ≥ 4.13.3, por isso o **gtkmm 4.14.0 foi compilado a partir do código-fonte** em `/opt/gtkmm414`.
- `cairo>=1.18.6` é exigido. O sistema tem 1.18.0, e `cairographics.org` está bloqueado no ambiente, então um `.pc` local apresentou o 1.18.0 como 1.18.6. Isso só serve para validar a compilação do nosso código.
- CMake: `-DWITH_CAPYPDF=ON` (interno). Desligados: POPPLER, IMAGE_MAGICK, GRAPHICS_MAGICK, LIBCDR, LIBVISIO, LIBWPG, LIBSPELLING e GSOURCEVIEW. Sem `WITH_CAPYPDF=ON`, o configure falha, porque `inkscape_base` sempre linka `Inkscape::CapyPDF`.
- Resultado: **935/935 etapas sem erro e sem avisos nos arquivos do Inksvector.** O aplicativo foi instalado e testado em Xvfb com xdotool.
- Isso não substitui o build MSYS2/Windows, que só roda com ordem do Álefe.

## 4. Correções que o Claude fez na área do Codex

### 4.1 Erro de compilação — `src/ui/toolbar/pencil-toolbar.cpp:222` (corrigido em `7457f06`)

- Origem: `3e8e869`. `hide_extra_widgets()` chamava `prefs->createObserver(...)`, mas `prefs` só existe como variável local em outras funções (linha 183 e o construtor).
- Erro do GCC: `error: 'prefs' was not declared in this scope`. **A `main` entre `3e8e869` e `7457f06` não compila**, e qualquer build Windows teria falhado.
- Correção: `Preferences::get()->createObserver(...)`. Nada mais foi alterado no arquivo.
- Revisei também a lógica nova: `update_mode()` via observador, guarda de `_desktop` nulo e visibilidade inicial do botão de achatar. Está consistente. Clicar no botão já ativo não gera laço, porque `setInt` com o mesmo valor não notifica e `set_active(true)` em botão ativo não emite `toggled`.

### 4.2 Exportador CMYK — `share/inksvector-extensions/inksvector_cmyk_pdf.py` (corrigido em `23efe48`)

- Sintoma: **toda exportação falhava** com `Unrecoverable error: undefined in .putdeviceprops` (Ghostscript 10.02.1).
- Causa: o `pdfwrite` usa `ProcessColorModel=DeviceRGB` por padrão e recusa um `-sOutputICCProfile` CMYK. Reproduzido também sem `-dSAFER` e sem `-P-`. Funciona só ao acrescentar `-sProcessColorModel=DeviceCMYK`.
- Correção: só essa opção foi acrescentada à lista `args`.

## 5. Pendências na área do Codex (com evidência)

### 5.1 CMYK: o preto 100% K vira preto composto — prioridade alta

Arquivo de teste: retângulo com `style="fill:#231f20 icc-color(press, 0, 0, 0, 1)"` e `<color-profile id="press" xlink:href=".../default_cmyk.icc"/>`.

| Etapa | Operador no conteúdo do PDF |
| --- | --- |
| PDF intermediário do Inksvector (CapyPDF) | `/CSpace4 cs` + `0 0 0 1 scn`: **correto**, CMYK em espaço ICC |
| Saída atual do Ghostscript | `0.678 0.671 0.651 0.741 k`: **preto composto** |
| `+ -dKPreserve=1` | `0.004 g`: só K, mas a 99,6% |
| `+ -sDefaultCMYKProfile=press.icc -dOverrideICC=true` | `0 0 0 1 k` e `1 0 0 0 k`: **exato**. O RGB continua sendo convertido (`#ff0000` → `0 0.996 1 0 k`) |

O `OverrideICC` ignora o perfil ICC embutido nos objetos. Com o mesmo perfil no documento e na saída, ele dá exatamente o resultado desejado. Se a gráfica usar outro perfil, os números CMYK passam sem conversão.

Sugestão: comparar o perfil do documento (`<color-profile>`) com o perfil escolhido, por hash MD5 do cabeçalho ICC ou do arquivo. Se forem iguais, usar `OverrideICC`. Se forem diferentes, manter a conversão e considerar `-dKPreserve=1`. Documentar na interface.

### 5.2 PDF/X-3 falha no Ghostscript 10.02.1 — prioridade alta

- Erro: `Error: /typecheck in --pdfmark--`, no `[ ... /DOCINFO pdfmark` do `output-intent.ps`.
- O exemplo oficial `/usr/share/ghostscript/10.02.1/lib/PDFX_def.ps`, ajustado só com o perfil, **falha igual**, com o interpretador novo e com `-dNEWPDF=false`. Remover a linha `/GTS_PDFXVersion` não resolve. É limitação dessa versão do Ghostscript.
- Ação: validar com um Ghostscript ≥ 10.03 (o Windows usará o que o usuário instalar). Se a versão instalada for problemática, detectar com `gs --version` e avisar. Não declarar conformidade PDF/X sem um validador independente.

### 5.3 Exportador CMYK — robustez

- O PDF intermediário é gerado com `inkex.command.inkscape(input, export_filename=...pdf)`. A extensão escolhida para `.pdf` depende da ordenação em `DB::get_output_list` (`src/extension/db.cpp`, `ModuleOutputCmp`), e o próprio `org.inksvector.output.cmyk-pdf` também declara `.pdf`. Hoje o exportador interno vence, porque o comparador prefere o que não é script, mas isso depende da ordem. **Passe `export_extension=` com o ID do exportador PDF interno** para eliminar o risco de recursão.
- O Ghostscript **não vem no instalador Windows**: `InstallMSYS2.cmake` não o empacota. Avisar no `.inx` e na mensagem de erro, ou decidir empacotá-lo, considerando que a licença é AGPL.
- O preto RGB `#000000` vira `0.722 0.675 0.671 0.882 k`. É o esperado pela conversão ICC, mas a interface deveria avisar que o preto puro exige cor CMYK.

### 5.4 Grades e Linhas com várias páginas — prioridade média

- Os 18 modos rodam num A4 em mm (`viewBox 0 0 210 297`). Valores conferidos à mão:
  - `cross`: guias em x=105 e y=148,5;
  - `margin` 10%: guias em x 21/189 e y 29,7/267,3;
  - `align-grid` com espaçamento 10: centro (21, 18) → (20, 20) e (61, 82) → (60, 80), inclusive dentro de grupo com `translate`;
  - `align`: alinha ao centro X do primeiro objeto;
  - `snap-guides` recusa sem guias.
- Grades usam `spacingx/originx` em unidades do usuário, consistente com `src/object/sp-grid.cpp`.
- Pendente: `get_page_bbox()` sem índice usa sempre a primeira página (achado do próprio Codex). Usar a página do objeto selecionado ou a página ativa.

### 5.5 Exclusão de segmento virtual e B-spline

- `virtual-segments.h` e `eraser-tool.cpp` compilam. Lógica revisada:
  - caixa da seleção convertida de vista para documento;
  - coordenadas locais restauradas por `(i2doc * doc2dt * d2w).inverse()`;
  - um passo de desfazer;
  - `Esc` cancela;
  - o destrutor solta o grab.
- **Não testado no aplicativo:** acionamento pelo GTK, objetos transformados, desfazer e formas paramétricas. Continua valendo o achado do Codex: `sp_item_list_to_curves` consolida LPE, retângulos e elipses em caminho. Decidir e informar isso na interface.
- O botão B-spline (`win.tool-bspline`) foi adicionado e compila. Não testei o desenho na interface.

### 5.6 Cores ICC CMYK (`5f506ce`)

- `color-set`, `color-page`, `color-picker-panel`, `icon-combobox`, `paint-switch` e `document-properties` compilam sem avisos.
- Mudança de comportamento a ter em mente: `PaintSwitchImpl::set_color` agora passa por `ColorPickerPanel::set_color`, que chama `ColorSet::set(color, false)`. O conjunto passa a adotar o espaço de cor recebido, em vez de converter para o atual. Isso é intencional para preservar ICC, mas convém testar a troca de objeto RGB ↔ CMYK com o seletor aberto.
- `testfiles/src/colors/color-set-test.cpp` **não foi executado** (o build foi feito com `BUILD_TESTING=OFF`). Rode a suíte de cores.

### 5.7 Ícones

`buildtools/media-check-icons.py`: os ícones novos (deslocamento ×4, `dialog-calculator`, `inksvector-delete-virtual-segment`) existem só em `hicolor`, sem cópia nos temas Dash e multicolor. O GTK usa `hicolor` como fallback, então eles aparecem, e a base já tinha 3 avisos iguais. O script retorna 0.

## 6. Mudanças do Claude na própria área (para conhecimento)

### Calculadora — `src/ui/dialog/calculator.{h,cpp}`

- **Menu e registro:** Extensões > MATH > Calculator..., pela ação `win.math-calculator`, que abre o diálogo `Calculator` flutuante. Também abre acoplado via `win.dialog-open('Calculator')`. Registro em `dialog-data.cpp`, `dialog-container.cpp` e `actions-dialogs.cpp`; menu em `menus.ui`.
- **Motor:** libqalculate, obrigatória (`libqalculate>=4.0` em `DefineDependsandFlags.cmake`). O MSYS2 instala `$ARCH-libqalculate`. O instalador Windows leva:
  - `libqalculate-*.dll`;
  - `share/qalculate`, procurado em `..\share\qalculate` a partir do `.exe`;
  - `libqalculate.mo`;
  - a licença.
- **Cuidados de integração:**
  - O construtor da libqalculate chama `setlocale(LC_ALL, "")` e pode definir `LANGUAGE`. Ambos são restaurados.
  - Uma thread ociosa da libqalculate bloqueada em `fread` causava **deadlock no `exit()`**, visto no gdb em `_IO_flush_all`. Corrigido com `terminateThreads()` em `atexit`. Validado: o Inksvector fecha em 2 s depois de usar a calculadora.
  - O download de câmbio usa uma flag atômica global: um download por vez, e os cálculos ficam bloqueados até `loadExchangeRates()` rodar na thread principal.
  - `ans` é guardado como `MathStructure`. Assim, `1/3` → `ans*3 = 1`, e um resultado exibido em hexadecimal não altera o valor numérico.
- `windows.h` (incluído pela libqalculate) pode definir as macros `min` e `max`: há `NOMINMAX`, e as chamadas `std::min`/`std::max` estão entre parênteses.

### Deslocamento — `src/ui/toolbar/select-toolbar.cpp`

- `GtkSpinButton` numérico com `LC_NUMERIC=C` descartava o que vinha depois da vírgula, e `2,5` movia 2 mm. Agora `signal_input` converte `,` em `.` e usa `g_ascii_strtod`.
- Testado no aplicativo:
  - setas;
  - diagonal (‑1,25/+1,25);
  - três Ctrl+Z para três cliques;
  - seleção múltipla, com a caixa de X 11 → 13,5 e a largura mantida em 55;
  - valores guardados por modo.

## 7. Não verificado por ninguém até agora

- Build MSYS2/Windows do estado atual, instalador, **versão portátil em ZIP** (ainda não implementada) e abertura num Windows x64 sem MSYS2.
- Testes automatizados do repositório (`BUILD_TESTING`).
- Interações GTK da Borracha (segmento virtual) e da B-spline.

## 8. Coordenação

- Divisão aceita: **Claude** fica com calculadora, libqalculate e deslocamento. **Codex** fica com CMYK, Grades e Linhas e a edição de caminhos (Borracha, B-spline).
- Não edite `src/ui/dialog/calculator.*` sem combinar.
- A cada entrega, acrescente uma seção datada em `doc/inksvector-review.md`, conforme o `CLAUDE.md`.
- Não dispare o workflow Windows sem ordem explícita do Álefe.
