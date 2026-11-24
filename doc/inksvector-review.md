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
