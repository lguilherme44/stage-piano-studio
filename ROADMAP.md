# Stage Piano Studio - Roadmap

Funcionalidades planejadas e marcos técnicos para as próximas versões.

## Fase 1: Importação Expandida de Amostras e Formatos
- [ ] **Importador Nativo de SoundFont 2 (`.sf2`)**: Leitura e reprodução direta de bancos SF2 clássicos sem necessidade de ferramentas externas ou conversão prévia.
- [ ] **Leitor de Formato SFZ**: Suporte a definições de texto `.sfz` para mapear multisamples customizados da comunidade.
- [ ] **Importador Drag & Drop de Pastas de Amostras**: Reconhecimento automático de nomes de arquivos (ex: `Grand_C3_v127.wav`) inferindo nota raiz, faixa de velocity e pontos de loop.
- [ ] **Compactação Lossless da Biblioteca**: Suporte a FLAC ou compactação sem perda em tempo real para reduzir o uso de disco das amostras.

## Fase 2: Realismo Acústico e Expressividade
- [ ] **Round-Robin Alternado**: Disparo cíclico de amostras diferentes na mesma nota/velocity para evitar o efeito "metralhadora" em repetições rápidas.
- [ ] **Amostras de Liberação (Release Samples) e Ressonância de Damper**: Acionamento de ruído mecânico na soltura da tecla e ressonância simpática ao pisar no pedal de sustain.
- [ ] **Perfis de Curva de Velocity**: Curvas selecionáveis (Suave, Linear, Pesada, Spline customizada) para casar perfeitamente com a ação de teclas de qualquer controlador (pesadas, semipesadas ou synth-action).

## Fase 3: Recursos para Performance ao Vivo
- [ ] **Transição Suave de Sons (Patch Remain / Sound Continuity)**: Sustentar acordes da música anterior sem corte de áudio ao trocar de preset.
- [ ] **Roteamento de Troca de Programa MIDI (PC)**: Seleção rápida de bancos e presets diretamente pelos botões físicos ou pedais do teclado.
- [ ] **Modo Divisão de Teclado (Keyboard Split)**: Ponto de divisão configurável para tocar Baixo/Drone na mão esquerda e Piano/Lead na mão direita.

## Fase 4: Ecossistema de Plugins
- [ ] **Versões VST3 e AU**: Gerar tanto o aplicativo Standalone quanto plugins para DAWs (Reaper, Logic, Ableton, Cubase) a partir do mesmo código CMake.
- [ ] **Modo Headless para Módulos Embarcados**: Execução ultraleve sem interface gráfica para uso em Raspberry Pi ou mini PCs como módulo de som dedicado em rack.
