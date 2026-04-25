# Relógio ESP32 + LCD 16×2 

Projeto para **ESP32** com **LCD HD44780 (4 bits)**, três botões e LED, com relógio 24 h, ajuste e cronómetro. O desenho do circuito está em `diagram.json`.

## Estrutura

| Ficheiro / pasta | Função |
|------------------|--------|
| `sketch/sketch.ino` | Código Arduino (sketch) |
| `diagram.json` | Esquemático Wokwi (ESP32, LCD, botões, LED) |
| `libraries.txt` | Lista de bibliotecas (Wokwi / referência) |
| `wokwi.toml` | Configuração do [Wokwi for VS Code](https://docs.wokwi.com/vscode/getting-started) (caminho do firmware) |
| `compile-wokwi.sh` | Compila o firmware localmente com `arduino-cli` |
| `build/` | Gerada após a compilação (contém `.bin` e `.elf`) — pode ir para `.gitignore` |

## Explicação simples do código

O sketch funciona como uma pequena máquina de estados:

1. `BOOT` - mostra a tela de inicialização por um instante.
2. `RUN` - mostra o relógio em tempo normal.
3. `CONFIG_H` - ajusta as horas.
4. `CONFIG_M` - ajusta os minutos.
5. `CRO` - cronómetro.

Em vez de usar `delay()`, o programa usa `millis()` para continuar lendo os botões e atualizando o LCD sem travar o Arduino.

### O que cada botão faz

- **B1**: entra no modo de ajuste; no ajuste de hora/minuto, passa para a próxima etapa; no cronómetro, volta para o relógio normal.
- **B2**: aumenta o valor em ajuste; no cronómetro, inicia ou pausa.
- **B3**: diminui o valor em ajuste; no cronómetro, zera.

### O que aparece no LCD

- Na tela normal, a primeira linha mostra a hora no formato `HH:MM:SS`.
- A segunda linha mostra mensagens curtas, como instruções ou confirmações.
- No modo de ajuste, o valor escolhido pisca para ficar fácil de ver.
- No cronómetro, o LED também ajuda a indicar o estado de execução.

### Como o programa está organizado no `sketch.ino`

- No início ficam os pinos, constantes de tempo e variáveis globais.
- Depois vêm funções pequenas e específicas, como leitura dos botões, atualização do relógio, controle do LED e desenho do LCD.
- No final ficam `setup()` e `loop()`, que apenas inicializam e chamam as rotinas principais.

## Pré-requisitos

1. **arduino-cli** (v1.4+ recomendado)  
   - [Instalação](https://arduino.github.io/arduino-cli/latest/installation/)  
   - Exemplo: `curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh`  
   - Garante que a pasta do executável está no `PATH` (ex.: `export PATH="$HOME/Downloads/bin:$PATH"` se instalaste aí).

2. **Núcleo ESP32** (Arduino):
   ```bash
   arduino-cli config init
   arduino-cli core update-index
   arduino-cli core install esp32:esp32
   ```

3. **Biblioteca LiquidCrystal** (o script instala automaticamente se faltar; ou manualmente):
   ```bash
   arduino-cli lib install "LiquidCrystal"
   ```

4. **Wokwi no VS Code** (simulação local)  
   - Extensão: *Wokwi for VS Code*  
   - Ativar licença: `F1` → **Wokwi: Request a new License** (conta Wokwi; [documentação](https://docs.wokwi.com/vscode/getting-started))  

## Compilar o firmware (local)

Na raiz desta pasta (onde estão `wokwi.toml` e `compile-wokwi.sh`):

```bash
./compile-wokwi.sh
```

Isto gera, entre outros, ficheiros em `build/`, em particular:

- `build/sketch.ino.merged.bin` — usado no `wokwi.toml` como `firmware`
- `build/sketch.ino.elf` — usado no `wokwi.toml` como `elf`

### Placa (FQBN)

O valor predefinido no script é **`esp32:esp32:esp32`** (“ESP32 Dev Module” no núcleo ESP32 3.x).  
Para usar outra placa listada no pacote `esp32:esp32`:

```bash
FQBN=esp32:esp32:esp32doit-devkit-v1 ./compile-wokwi.sh
```

(Confirma FQBNs com `arduino-cli board listall esp32:esp32`.)

### Se o caminho em `build/` for diferente

Raras vezes, outra versão do `arduino-cli` ou FQBN pode colocar os ficheiros noutra subpasta. Abre `build/`, localiza o `.merged.bin` (ou `.bin`) e o `.elf` e ajusta as chaves `firmware` e `elf` no ficheiro `wokwi.toml` com caminhos relativos à raiz do projeto, usando sempre `/` como separador.

Se a simulação recusar o `merged`, experimenta trocar `firmware` para `build/sketch.ino.bin` no `wokwi.toml`.

## Simular com Wokwi (VS Code)

1. **Abre esta pasta** como raiz de trabalho: `Ficheiro` → *Abrir pasta* → seleciona `wokwi-relogio-esp32`.
2. Garante que já correste **`./compile-wokwi.sh`** e que `build/` contém os ficheiros esperados.
3. `F1` (ou Ctrl+Shift+P) → **Wokwi: Start Simulator**.

## Wokwi no browser (wokwi.com)

Podes colar o conteúdo de `sketch/sketch.ino` no projecto online e o `diagram.json` correspondente. A compilação aí depende dos **servidores** do Wokwi; se aparecer *Build servers busy*, tenta mais tarde, ou usa a simulação local com o VS Code, como acima.

## Referência rápida: pinos (não mudar o esquemático do enunciado)

| Função   | GPIO |
|----------|------|
| LCD RS, E, D4–D7 | 21, 22, 19, 18, 5, 17 |
| B1, B2, B3        | 27, 26, 25   |
| LED              | 2  |

Documentação Wokwi: <https://docs.wokwi.com/vscode/project-config> · <https://docs.wokwi.com/guides/libraries>
