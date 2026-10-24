# Configuração de Teclados & Controladores MIDI

O **Stage Piano Studio** é *class-compliant* e conecta-se automaticamente a qualquer teclado USB MIDI ou interface conectada ao computador.

## Início Rápido

1. Conecte seu teclado ou interface USB ao computador.
2. Se o seu teclado tiver uma opção de driver no menu interno (ex: `Vender` vs `Generic`), certifique-se de deixá-la em **`Generic` / `Class-Compliant`**.
3. Abra o Stage Piano Studio.
4. Clique no botão **`AUDIO / MIDI`** (ou **`CONFIG`** no rodapé) para abrir as configurações.
5. Na lista **Active MIDI Inputs**, marque a caixa do seu teclado.

## macOS
O CoreMIDI detecta teclados USB conectados automaticamente sem precisar de driver extra. Ao tocar uma nota, o indicador no rodapé atualiza exibindo o nome do dispositivo.

## Windows
Os drivers MIDI do Windows e interfaces WASAPI/ASIO são listados diretamente no painel de configurações. Escolha o driver de menor latência (ASIO dedicado da interface ou Windows Audio Exclusive) para obter o menor atraso possível (buffer entre 64 e 128 amostras).

## Linux
Portas ALSA e JACK MIDI são reconhecidas de forma nativa. Certifique-se de que o seu usuário pertence ao grupo `audio` no sistema para permissões de prioridade em tempo real.
