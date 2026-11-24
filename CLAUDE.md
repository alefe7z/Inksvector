# Inksvector — colaboração Claude e Codex

Leia a [revisão técnica compartilhada](doc/inksvector-review.md) antes de iniciar alterações. Ela registra os achados priorizados, o que já foi testado e a divisão de trabalho proposta para Claude e Codex.

## Branch compartilhada

Trabalhe na `main` deste repositório. Antes de editar, atualize a cópia local com `git pull --ff-only origin main` e leia os commits recentes. Se o checkout estiver sujo, inspecione e preserve as alterações antes de atualizar. Combine a divisão de arquivos com o outro agente antes de editar as mesmas áreas. Faça commits pequenos, rode as verificações pertinentes e registre nos commits o que mudou e o que foi testado. Confira novamente a ponta da `main` antes do push. Se houver sobreposição ou o push for rejeitado, revise o diff e integre o trabalho publicado antes de tentar novamente. Evite force push na branch compartilhada.

## Base e estrutura

O ZIP enviado pelo Álefe corresponde ao Inkscape 1.5-dev, commit `82ace79ad91b0af7e8fc4b93e194c64a5f17fd67` de https://gitlab.com/inkscape/inkscape.git. A `main` contém a árvore completa de código, os arquivos da ferramenta de deslocamento e o workflow do Windows. Use `git clone --recurse-submodules https://github.com/alefe7z/Inksvector.git` ou, em uma cópia já existente, `git submodule update --init --recursive` após atualizar a branch. Preserve os commits dos submódulos e os arquivos de licença.

## Deslocamento

O commit `885673f94449367327d6e87536ae64132b802186` adicionou o botão de deslocamento à barra da ferramenta Seleção.

- Código e interface em `src/ui/toolbar/select-toolbar.cpp` e `share/ui/toolbar-select.ui`.
- Quatro ícones SVG em `share/icons/hicolor/symbolic/actions/selection-displacement*-symbolic.svg`.
- Passo único oferece as quatro setas cardinais com um mesmo valor. Horizontal oferece esquerda e direita. Vertical oferece cima e baixo. Diagonal oferece quatro setas e aplica a distância em cada eixo.
- Use milímetros e mantenha valores independentes por modo, seleção múltipla e um passo de desfazer por clique.
- A prévia em português era ilustrativa. Teste a interface compilada, inclusive entrada decimal, seleção múltipla, sinais dos eixos, repetição e Ctrl+Z.

## Calculadora (Extensões > MATH)

Menu Extensões > MATH > Calculator..., que abre uma janela flutuante (`win.math-calculator`). O diálogo também pode ser acoplado (`win.dialog-open('Calculator')`). O motor é a libqalculate, a mesma biblioteca do Qalculate!. A interface GTK do Qalculate! é GTK3 e não pode ser embutida no Inkscape GTK4. Por isso o diálogo é nativo GTK4 e chama a biblioteca.

- Código em `src/ui/dialog/calculator.{h,cpp}`. Registro em `src/ui/dialog/dialog-data.cpp`, `src/ui/dialog/dialog-container.cpp` e `src/actions/actions-dialogs.cpp`. Menu em `share/ui/menus.ui`. Ícone `dialog-calculator` em `share/icons/hicolor/{scalable,symbolic}/actions/`.
- Dependência obrigatória `libqalculate>=4.0` em `CMakeScripts/DefineDependsandFlags.cmake`. No MSYS2, o pacote `$ARCH-libqalculate` fica em `buildtools/msys2installdeps.sh`. O pacote Windows (`CMakeScripts/InstallMSYS2.cmake`) copia `libqalculate-*.dll`, `share/qalculate` (definições de unidades, funções e moedas, procuradas em `..\share\qalculate` a partir do `.exe`), as traduções e a licença.
- O construtor da libqalculate chama `setlocale(LC_ALL, "")` e pode definir `LANGUAGE` no Windows. `ensure_engine()` restaura os dois. Mantenha esse cuidado.
- Testado fora do Inkscape (gtkmm 4.10, libqalculate 4.9, Ubuntu), com stubs de `DialogBase` e `Preferences`: aritmética, `ans`, unidades, bases, graus, modo exato, vírgula decimal, precisão, histórico e teclado. Ainda falta testar a janela no Inkscape compilado no Windows.

## Entrega Windows x64

O GitHub Actions usa MSYS2 UCRT64, CMake, Ninja e NSIS. A execução de referência da base concluiu com sucesso em https://github.com/alefe7z/Inksvector/actions/runs/45382395012. A execução com deslocamento está em https://github.com/alefe7z/Inksvector/actions/runs/45403533374. Consulte o status e os logs antes de editar o workflow.

Gere executáveis Windows somente quando Álefe pedir explicitamente. O workflow é manual (`workflow_dispatch`). Commits e pushes não devem disparar a geração de executáveis.

Sempre que Álefe solicitar um `.exe`, a entrega deve incluir o instalador e uma versão portátil que abra com dois cliques, sem instalação. Se a portátil depender de DLLs, ícones ou recursos, entregue um ZIP com a estrutura inteira e indique qual executável abrir após extrair. O empacotamento portátil ainda está pendente. Valide a abertura em Windows x64 sem MSYS2 instalado e inclua SHA-256 dos artefatos.
