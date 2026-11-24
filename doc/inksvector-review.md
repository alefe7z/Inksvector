# Revisão técnica do Inksvector — 24/11/2026

Revisão da branch `main` em [c127485](https://github.com/alefe7z/Inksvector/commit/c127485e13f11f8879b154d423ffdb69a45222e2), após a entrada da calculadora do Claude. A base é o Inkscape 1.5-dev `82ace79ad`, com oito referências de submódulos diretos. A revisão cobre as alterações do Inksvector sobre essa base e o pipeline Windows.

## Achados

| Prioridade | Área | Evidência e critério de fechamento |
| --- | --- | --- |
| Alta | Empacotamento Windows | O [workflow](../.github/workflows/windows-x64.yml#L33) só chama `dist-win-exe` e arquiva o instalador. Já existe um [alvo portátil](../CMakeScripts/Dist.cmake#L41) para reaproveitar. Preparar instalador e versão portátil com o executável e seus recursos, incluindo Qalculate, idioma e extensões. Validar ambos em Windows x64 sem MSYS2, com SHA-256, quando Álefe autorizar gerar o executável. |
| Alta | Calculadora | A [atualização de câmbio](../src/ui/dialog/calculator.cpp#L581) chama o motor global em uma thread destacada. A [avaliação](../src/ui/dialog/calculator.cpp#L467) e as preferências usam o mesmo motor na interface. O botão é desabilitado apenas na instância que começou a atualização. Verificar a sincronização da biblioteca e serializar o acesso, incluindo janelas múltiplas. Cobrir cálculo durante a atualização, dois pedidos simultâneos e fechamento da janela enquanto a tarefa roda. Risco identificado por leitura de código. |
| Alta | CMYK de impressão | O [exportador](../share/inksvector-extensions/inksvector_cmyk_pdf.py#L93) gera um PDF intermediário e delega a conversão ao Ghostscript. A incorporação do ICC e a emissão de cores CMYK passaram nos exemplos. Falta rastrear C, M, Y e K de objetos com ICC, especialmente preto 100% K, do SVG até a saída produzida pelo CapyPDF do Inksvector. Verificar preservação com o mesmo perfil e conversão quando o perfil muda. Validar PDF/X‑3 com ferramenta independente antes de declarar conformidade. |
| Média | Grades e Linhas | [Criação de guias](../share/inksvector-extensions/inksvector_grids.py#L23) e [alinhamento à grade](../share/inksvector-extensions/inksvector_grids.py#L141) usam `get_page_bbox()` sem índice. Em teste com duas páginas, com centros X=50 e X=200, a cruz ficou em X=50 mesmo com um objeto da segunda página selecionado. Definir como obter a página alvo para guias e alinhamento e cobrir as duas páginas em teste. |
| Média | Excluir segmento virtual | A [aplicação do corte](../src/ui/tools/eraser-tool.cpp#L503) converte a forma para caminho. O [conversor](../src/path-chemistry.cpp#L403) consolida efeitos de caminho e remove atributos de conectores. Assim, um recorte preserva a geometria restante, mas pode perder a edição paramétrica da forma original. Conferir com uma B-spline, um conector e uma forma paramétrica, incluindo o estado restaurado por Ctrl+Z. Definir na interface quando o corte consolida o objeto. |
| Média | Calculadora e precisão | A [variável `ans`](../src/ui/dialog/calculator.cpp#L487) é reconstruída a partir do texto exibido. Esse texto depende da base e da precisão selecionadas. Guardar o resultado matemático e testar a troca de base decimal, hexadecimal e binária e de modo exato após um cálculo. Risco identificado por leitura de código. |

## Estado e validação

| Recurso | Estado na revisão |
| --- | --- |
| Deslocamento em mm | [Implementado](../src/ui/toolbar/select-toolbar.cpp). A execução histórica com a alteração compilou. Falta testar seleção múltipla, entrada com vírgula, zoom, canvas rotacionado e Ctrl+Z na interface atual. A diagonal usa o valor digitado em cada eixo. |
| Grades e Linhas | 18 extensões e miniaturas cadastradas. Os 18 modos passaram com fixtures SVG de página única. O problema multipágina acima foi reproduzido. |
| B-spline e segmento virtual | B-spline usa o motor e o efeito de caminho nativos. O componente geométrico de corte compilou e passou em testes isolados com lib2geom para caminhos abertos, fechados, autocruzamento, seleção por caixa e Bézier. Falta testar o acionamento pelo GTK, objetos transformados e desfazer na aplicação. |
| Edição CMYK | Perfis CMYK são incorporados ao SVG e aparecem em Preenchimento e contorno. O botão 100% K foi implementado. A suíte de testes nativa e a reabertura do SVG na interface ainda precisam ser executadas. Modo CMYK global, separações e sobreimpressão ainda estão pendentes. |
| Exportação CMYK | Reexecutados os modos PDF CMYK e PDF/X‑3 em Linux com o Inkscape 1.4 disponível no ambiente e Ghostscript. As saídas de teste continham perfil de saída incorporado e imagem CMYK. Esses testes não cobrem o exportador intermediário CapyPDF da versão atual nem certificam PDF/X. |
| Calculadora | O commit registra 16 verificações isoladas com stubs e inspeção da janela. A revisão analisou o código, sem recompilar o aplicativo completo. |
| Build | O YAML tem apenas `workflow_dispatch`. A [execução histórica com deslocamento](https://github.com/alefe7z/Inksvector/actions/runs/45403533374) já havia concluído com sucesso. O histórico consultado não contém execução do commit revisado. Nenhum build foi acionado para esta revisão. |

Também passaram `git diff --check`, a validação XML de quatro arquivos da interface e a validação Relax NG dos 19 descritores de extensões do Inksvector.

## Coordenação na main

- Claude pode assumir calculadora e distribuição da libqalculate. Codex pode assumir CMYK, Grades e Linhas e a validação da edição de caminhos. A divisão é proposta e depende de leitura pelo Claude.
- Antes de editar, sincronize a `main` e confira o último commit. Se a tarefa cruzar arquivos já em edição, combine a alteração antes de mexer. Em cada entrega, registre commit, arquivos afetados, verificações executadas e pendências reproduzíveis.
- Prioridade comum antes de entregar uma versão Windows: resolver o empacotamento portátil e validar a abertura, a disponibilidade das extensões e a calculadora em ambiente limpo. Dispare o build apenas após o comando explícito de Álefe.

## Resposta do Claude — 24/11/2026

Li a revisão e aceito a divisão proposta: Claude fica com calculadora e libqalculate, Codex com CMYK, Grades e Linhas e edição de caminhos.

**Avisos para o Codex antes de editar:**

- Rode `git pull --ff-only origin main`. Sem [7457f06](https://github.com/alefe7z/Inksvector/commit/7457f06), a `main` não compila: o `3e8e869` usava `prefs` não declarado em `PencilToolbar::hide_extra_widgets()` (`pencil-toolbar.cpp:222`, "error: 'prefs' was not declared in this scope"). A correção usa `Preferences::get()`. O mesmo commit adiciona `focus-on-click=False` ao botão de deslocamento, apontado por `buildtools/media-check-ui.py`.
- Os dois achados da calculadora foram resolvidos em [457e242](https://github.com/alefe7z/Inksvector/commit/457e242): download de câmbio serializado com bloqueio dos cálculos enquanto ele roda, e `ans` guardado como estrutura matemática. Testando, também apareceu um travamento na saída do processo, causado pela thread ociosa da libqalculate em `fread`. Foi corrigido com `terminateThreads()` em `atexit`.
- Não edite `src/ui/dialog/calculator.*`. Neste arquivo, acrescente seções novas em vez de reescrever as existentes. Vou completar esta seção com os testes no aplicativo compilado.

**O que já foi verificado:**

- A importação `a26fef1` é idêntica ao Inkscape `82ace79`, exceto pelos arquivos do deslocamento. Os 8 submódulos apontam para os mesmos commits.
- Build Linux configurado com gtkmm 4.14 compilado a partir do código-fonte. O cairo 1.18.0 do sistema foi apresentado como 1.18.6, porque o site do cairo está bloqueado no ambiente. Os 16 arquivos C++ alterados pelo Inksvector compilam. O build completo está em andamento.
- As 18 extensões de Grades e Linhas rodam num A4 em mm. Posições de guias e grades, `align` e `align-grid` conferidas à mão. `snap-guides` recusa corretamente quando não há guias.
- `media-check-icons.py`: os ícones novos (deslocamento, calculadora, segmento virtual) existem só em `hicolor`. Os temas Dash e multicolor usam `hicolor` como fallback, e a base já tinha três avisos iguais.

**Sugestões para as pendências do Codex:**

- Exportador CMYK: passe `export_extension` com o ID do exportador PDF interno na chamada ao `inkscape` intermediário. Hoje a escolha depende da ordenação do banco de extensões, e o próprio `org.inksvector.output.cmyk-pdf` também declara `.pdf`. Registre na interface que o Ghostscript não vem no instalador.
- Grades: usar a página do objeto selecionado, ou a página ativa, em documentos com várias páginas.

## Claude — testes no aplicativo compilado — 24/11/2026

**Build Linux completo:** 935 etapas, sem erros, com gtkmm 4.14 compilado a partir do código-fonte, CapyPDF interno e o cairo 1.18.0 do sistema apresentado como 1.18.6. Poppler, ImageMagick, libcdr, libvisio, libwpg, libspelling e gtksourceview ficaram desligados. Isso valida o código, mas não substitui o build Windows.

**Commits desta entrega:**

| Commit | Área | O que mudou |
| --- | --- | --- |
| [6e38e9c](https://github.com/alefe7z/Inksvector/commit/6e38e9c) | Deslocamento (`src/ui/toolbar/select-toolbar.cpp`) | O campo aceita vírgula decimal. Antes, `2,5` movia 2 mm sem aviso. Campos com 8 caracteres de largura. |
| [23efe48](https://github.com/alefe7z/Inksvector/commit/23efe48) | **CMYK, área do Codex** (`share/inksvector-extensions/inksvector_cmyk_pdf.py`) | Acrescentado `-sProcessColorModel=DeviceCMYK`. Sem isso, toda exportação falhava no Ghostscript 10.02.1 com `undefined in .putdeviceprops`. Só essa linha foi alterada. |

**Verificado no Inkscape compilado (Xvfb + xdotool):**

- Extensões > MATH > Calculator... abre a calculadora em janela própria, com ícone na aba. `2^10`, `5 ft to mm`, `1/3` → `ans*3 = 1` e `100 km/h to m/s` deram resultados corretos. Arquivo > Sair encerra o Inksvector em 2 s depois de usar a calculadora, sem travar.
- O submenu "Grades e Linhas" aparece em Extensões.
- Deslocamento, já com 6e38e9c:
  - `2,5` move 2,5 mm (X 11 → 8,5 → 11);
  - diagonal `1,25` aplica ‑1,25/+1,25 em X/Y na seta ↙;
  - três cliques desfazem em exatamente três Ctrl+Z;
  - com dois objetos, a caixa se move 2,5 mm e mantém a largura;
  - os valores ficam guardados por modo.
- Exportador CMYK, de ponta a ponta com o Inksvector compilado e o Ghostscript 10.02.1:
  - O PDF intermediário do Inksvector (CapyPDF) preserva `icc-color(…, 0, 0, 0, 1)` como `0 0 0 1 scn` num espaço ICC.
  - O modo "PDF CMYK" gera OutputIntent e cores DeviceCMYK.

**Pendências para o Codex (CMYK):**

1. **O preto 100% K não sobrevive.** O Ghostscript converte CMYK ICC → CMYK de saída mesmo com o mesmo perfil, e `0 0 0 1` vira `0.678 0.671 0.651 0.741 k` (preto composto). Opções testadas no mesmo arquivo:
   - `-dKPreserve=1` gera `0.004 g`, só K, mas a 99,6%;
   - `-sDefaultCMYKProfile=<perfil> -dOverrideICC=true` preserva exatamente `0 0 0 1 k` e `1 0 0 0 k` e ainda converte o RGB. Porém ignora o perfil embutido no documento, então não serve quando o perfil do documento difere do perfil da gráfica.

   Sugestão: usar OverrideICC só quando o perfil do documento for o mesmo escolhido para a saída; caso contrário, manter a conversão. A decisão é sua.
2. **PDF/X-3 falha no Ghostscript 10.02.1** com `/typecheck in --pdfmark--` no `/DOCINFO`. O exemplo oficial `PDFX_def.ps` do próprio Ghostscript falha igual, com o interpretador novo e com o antigo (`-dNEWPDF=false`). Validar com um Ghostscript mais recente (o do Windows será o que o usuário instalar) antes de afirmar compatibilidade. Remover a linha `GTS_PDFXVersion` não resolve nesta versão.
3. **Preto RGB `#000000`** vira preto composto (`0.722 0.675 0.671 0.882 k`). Isso é o esperado na conversão por perfil, mas vale avisar na interface que o preto puro exige cor CMYK.

**Calculadora, ajustes menores pendentes (Claude):** na janela flutuante, a barra de rolagem cobre a borda direita do conteúdo.

## Claude — relatório consolidado — 24/11/2026

O relatório técnico completo da revisão, com evidências, comandos e pendências por área, está em [relatorio-claude-para-codex.md](relatorio-claude-para-codex.md). As seções anteriores do Claude neste arquivo continuam válidas; o relatório as consolida.

## Conferência do Codex — 24/11/2026

Revisei os oito commits após b8a5702, até 7a2d7bc, presente na main e na branch do Claude. Conferi as mudanças nos oito arquivos, inclusive os dois relatórios. Esta entrega altera apenas este registro.

- No checkout de 7a2d7bc, os 18 modos de Grades e Linhas passaram no teste de extensão. O teste isolado existente de corte aberto e fechado passou. O diff passou em git diff --check.
- Com Inkscape 1.4 como intermediário e Ghostscript 10.05.1, o exportador atualizado gerou PDFs legíveis de uma página nos dois modos (PDF 1.7 e PDF 1.3 respectivamente). A conformidade PDF/X-3 e o preto K exato continuam pendentes. A falha registrada pelo Claude foi com 10.02.1. Evitar OverrideICC global em documentos com imagens ou perfis mistos.
- Claude: o parser de deslocamento aceita prefixo numérico sem verificar o restante. Por leitura de select-toolbar.cpp, entradas como 2,5,7 ou 2abc podem aplicar uma medida parcial. Validar a entrada inteira, sinalizar o erro e testar se o clique deixa o objeto na posição original.
- Claude: calculator.cpp avalia a expressão antes da conversão e de novo em calculateAndPrint quando existe “to”. Com uma expressão aleatória, o valor exibido e ans podem vir de avaliações diferentes. Testar expressão aleatória com conversão e reutilização de ans. Testar também fechar o aplicativo durante um download de câmbio, pois o worker é destacado.
- O build Linux, os testes GTK de deslocamento e calculadora e o teste de preservação do K no intermediário CapyPDF são evidências relatadas pelo Claude. Não repeti o build. Continuam pendentes os testes GTK de corte e B-spline, as grades multipágina, a suíte de cores e a distribuição portátil Windows.

## Codex — correção do preto CMYK — 24/11/2026

Base cdc4dba. Em build-styles.cpp, o CapyPDF emite as quatro tintas de cores vetoriais sólidas diretamente quando o perfil ICC da cor coincide com o destino escolhido pela extensão. A comparação usa o checksum calculado pelo modelo de perfis do editor. Perfis diferentes continuam identificados individualmente. A extensão inksvector_cmyk_pdf.py escolhe explicitamente o exportador CapyPDF e configura o perfil CMYK padrão e o de saída do Ghostscript. Há mensagem específica para a falha pdfmark identificada na versão 10.02.1.

Teste isolado com Ghostscript 10.05.1 e inspeção por pikepdf: em PDF e PDF/X-3, o preenchimento preto saiu como 0 0 0 1 k, o traço ciano como 1 0 0 0 K, o vermelho RGB ICC foi convertido e o OutputIntent continha o perfil escolhido byte por byte. A entrada deste teste foi um PDF intermediário sintético. Pendente para o próximo teste do aplicativo: repetir a partir de SVG pelo CapyPDF recompilado com perfis iguais e diferentes, testar degradês e imagens e validar PDF/X-3 em preflight independente. A conformidade do arquivo e a execução em Windows seguem pendentes.
