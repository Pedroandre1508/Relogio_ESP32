// ESP32 + LCD 16x2
// Pinos do circuito:
// - LCD: RS=21, E=22, D4-D7=19,18,5,17
// - Botoes: B1=27, B2=26, B3=25
// - LED: 2
// O sketch usa millis() em vez de delay() para manter o loop responsivo.
#include <LiquidCrystal.h>

const int PINO_LCD_RS = 21;
const int PINO_LCD_E = 22;
const int PINO_LCD_D4 = 19;
const int PINO_LCD_D5 = 18;
const int PINO_LCD_D6 = 5;
const int PINO_LCD_D7 = 17;

const int PINO_BOTAO_HORA = 27;
const int PINO_BOTAO_MINUTO = 26;
const int PINO_BOTAO_ZERAR = 25;
const int PINO_LED = 2;

const int PINOS_BOTOES[3] = { PINO_BOTAO_HORA, PINO_BOTAO_MINUTO, PINO_BOTAO_ZERAR };

const uint32_t TEMPO_DEBOUNCE_MS = 50;
const uint32_t TEMPO_PRESSAO_LONGA_MS = 1200;
const uint32_t TEMPO_FEEDBACK_MS = 800;
const uint32_t TEMPO_PISCA_MS = 500;
const uint32_t TEMPO_PULSO_LED_MS = 200;
const uint32_t TEMPO_REDESENHO_CRONO_MS = 100;
// Resumo rapido dos tempos:
// - TEMPO_DEBOUNCE_MS: filtra ruido mecanico dos botoes
// - TEMPO_PRESSAO_LONGA_MS: tempo para "segurar" o B2 e abrir cronometro
// - TEMPO_FEEDBACK_MS: duracao das mensagens temporarias no LCD

// Cada estado representa uma tela ou modo diferente do projeto.
enum EstadoSistema { ESTADO_BOOT, ESTADO_RUN, ESTADO_AJUSTE_HORA, ESTADO_AJUSTE_MINUTO, ESTADO_CRONO };
// ESTADO_BOOT: tela inicial
// ESTADO_RUN: relogio rodando
// ESTADO_AJUSTE_HORA/ESTADO_AJUSTE_MINUTO: ajuste de hora e minuto
// ESTADO_CRONO: cronometro

// Variáveis globais do relógio, ajuste e cronômetro.
static EstadoSistema estado = ESTADO_BOOT;
static uint32_t bootAteMs;
static uint32_t segundosRelogio;
static uint32_t ultimoTickSegundoMs;
static uint32_t ledAcesoAteMs;
static uint32_t feedbackAteMs;
static uint32_t redesenhoCronoMs;
static uint32_t piscaUltimaTrocaMs;
static uint32_t cronoAcumuladoMs;
static uint32_t cronoInicioMs;
static int horaEdicao;
static int minutoEdicao;
static bool pausado = true;
static bool cronoRodando;
static bool piscaVisivel = true;
static char linhaFeedback[17];

// Guarda o estado de cada botão para tratar ruído e pressionamento longo.
struct EstadoBotao {
  int nivelBruto;
  int nivelEstavel;
  uint32_t ultimaMudancaMs;
  uint32_t inicioPressaoMs;
  bool pressaoLongaTratada;
};

static EstadoBotao botoes[3];
LiquidCrystal lcd(PINO_LCD_RS, PINO_LCD_E, PINO_LCD_D4, PINO_LCD_D5, PINO_LCD_D6, PINO_LCD_D7);

// Imprime sempre dois dígitos, por exemplo 03 ou 18.
static void print2Digits(int valor) {
  valor %= 100;
  lcd.print((char)('0' + valor / 10));
  lcd.print((char)('0' + valor % 10));
}

// Imprime um dígito único, usado na fração de segundo do cronômetro.
static void print1Digit(int valor) {
  valor %= 10;
  lcd.print((char)('0' + valor));
}

// Calcula o tempo total do cronômetro, somando a parte já acumulada com a parte em execução.
static uint32_t chronoMillis() {
  return cronoRodando ? cronoAcumuladoMs + (millis() - cronoInicioMs) : cronoAcumuladoMs;
}

// Pisca o LED quando o relógio fecha uma hora exata.
static void hourChime() {
  int minuto = (segundosRelogio / 60) % 60;
  int segundo = (int)(segundosRelogio % 60);

  if (minuto == 0 && segundo == 0) {
    ledAcesoAteMs = millis() + TEMPO_PULSO_LED_MS;
  }
}

void handleButtonRelease(int indiceBotao, uint32_t duracaoPressaoMs);

// Guarda uma mensagem curta na segunda linha do LCD por alguns milissegundos.
static void showFeedback(const char* texto) {
  uint8_t indice = 0;

  for (; indice < 16 && texto[indice]; indice++) {
    linhaFeedback[indice] = (char)texto[indice];
  }
  for (; indice < 16; indice++) {
    linhaFeedback[indice] = ' ';
  }

  linhaFeedback[16] = 0;
  feedbackAteMs = millis() + TEMPO_FEEDBACK_MS;
}

// Escreve a mensagem salva acima, completando sempre os 16 caracteres da tela.
static void printFeedbackLine() {
  for (int indice = 0; indice < 16; indice++) {
    lcd.print(linhaFeedback[indice]);
  }
}

// Inicializa os botões em repouso, como se nenhum tivesse sido pressionado ainda.
static void initButtons() {
  for (int indice = 0; indice < 3; indice++) {
    botoes[indice].nivelBruto = 1;
    botoes[indice].nivelEstavel = 1;
    botoes[indice].ultimaMudancaMs = 0;
    botoes[indice].inicioPressaoMs = 0;
    botoes[indice].pressaoLongaTratada = false;
  }
}

// Faz o texto piscar no modo de ajuste para indicar qual valor está sendo editado.
static void updateBlink() {
  if (millis() - piscaUltimaTrocaMs > TEMPO_PISCA_MS) {
    piscaUltimaTrocaMs = millis();
    piscaVisivel = !piscaVisivel;
  }
}

// Lê os botões, aplica debounce e detecta pressionamento curto e longo.
void bC() {
  uint32_t agora = millis();

  // index 0 = B1, index 1 = B2, index 2 = B3.
  for (int indice = 0; indice < 3; indice++) {
    // INPUT_PULLUP: botao solto = HIGH (1), pressionado = LOW (0).
    int nivelBruto = digitalRead(PINOS_BOTOES[indice]) == LOW ? 0 : 1;

    if (nivelBruto != botoes[indice].nivelBruto) {
      // Mudanca bruta detectada; inicia a janela de debounce.
      botoes[indice].nivelBruto = nivelBruto;
      botoes[indice].ultimaMudancaMs = agora;
    }

    // Ignora variacoes dentro da janela de estabilizacao.
    if (agora - botoes[indice].ultimaMudancaMs < TEMPO_DEBOUNCE_MS) {
      continue;
    }

    // Se o estado estavel nao mudou, nao ha evento novo.
    if (botoes[indice].nivelBruto == botoes[indice].nivelEstavel) {
      continue;
    }

    int nivelEstavelAnterior = botoes[indice].nivelEstavel;
    botoes[indice].nivelEstavel = botoes[indice].nivelBruto;

    if (nivelEstavelAnterior == 1 && botoes[indice].nivelEstavel == 0) {
      // Borda de descida: comecou o pressionamento.
      botoes[indice].inicioPressaoMs = agora;
      botoes[indice].pressaoLongaTratada = false;
    } else if (nivelEstavelAnterior == 0 && botoes[indice].nivelEstavel == 1) {
      // Borda de subida: terminou o pressionamento, calcula duracao.
      handleButtonRelease(indice, agora - botoes[indice].inicioPressaoMs);
      botoes[indice].inicioPressaoMs = 0;
    }
  }

  // No modo relogio, segurar B2 por TEMPO_PRESSAO_LONGA_MS entra no cronometro.
  if (estado == ESTADO_RUN && botoes[1].nivelEstavel == 0 && botoes[1].inicioPressaoMs) {
    if (!botoes[1].pressaoLongaTratada && agora - botoes[1].inicioPressaoMs >= TEMPO_PRESSAO_LONGA_MS) {
      botoes[1].pressaoLongaTratada = true;
      // Ao entrar no cronometro, ele comeca pausado e zerado.
      estado = ESTADO_CRONO;
      pausado = true;
      cronoAcumuladoMs = 0;
      cronoInicioMs = 0;
      cronoRodando = false;
      showFeedback("> Cron.     ");
      redesenhoCronoMs = 0;
    }
  }
}

// Avança o relógio uma vez por segundo, somente quando ele está rodando.
void tickClock() {
  if (pausado || estado != ESTADO_RUN) {
    return;
  }

  uint32_t agora = millis();

  // while evita perder segundos se o loop atrasar por algum motivo.
  while (agora - ultimoTickSegundoMs >= 1000) {
    ultimoTickSegundoMs += 1000;
    segundosRelogio++;

    // Mantem o relogio em ciclo de 24h.
    if (segundosRelogio >= 86400) {
      segundosRelogio = 0;
    }

    hourChime();
  }
}

// Controla o LED: pulso na virada de hora, aceso no ajuste e ligado/desligado no cronômetro.
void updateLed() {
  uint32_t agora = millis();

  // Prioridade 1: pulso curto na virada da hora.
  if (agora < ledAcesoAteMs) {
    digitalWrite(PINO_LED, HIGH);
    return;
  }

  // Prioridade 2: nos modos de ajuste o LED fica aceso fixo.
  if (estado == ESTADO_AJUSTE_HORA || estado == ESTADO_AJUSTE_MINUTO) {
    digitalWrite(PINO_LED, HIGH);
    return;
  }

  // Prioridade 3: no cronometro o LED indica rodando (aceso) ou pausado (apagado).
  if (estado == ESTADO_CRONO) {
    digitalWrite(PINO_LED, cronoRodando ? HIGH : LOW);
    return;
  }

  digitalWrite(PINO_LED, LOW);
}

// Decide o que cada botão faz dependendo do modo atual.
void handleButtonRelease(int indiceBotao, uint32_t duracaoPressaoMs) {
  if (estado == ESTADO_BOOT) {
    // Durante o boot, qualquer toque e ignorado.
    return;
  }

  // Se B2 ja abriu o cronometro por pressionamento longo, ignora o "soltar".
  if (indiceBotao == 1 && duracaoPressaoMs >= TEMPO_PRESSAO_LONGA_MS && estado == ESTADO_CRONO) {
    return;
  }

  if (indiceBotao == 0) {
    // B1: navega entre etapas e confirma/encerra modos.
    if (estado == ESTADO_RUN) {
      // Copia hora atual para variaveis de edicao antes de entrar no ajuste.
      horaEdicao = (segundosRelogio / 3600) % 24;
      minutoEdicao = (segundosRelogio / 60) % 60;
      estado = ESTADO_AJUSTE_HORA;
      pausado = true;
      piscaUltimaTrocaMs = millis();
      piscaVisivel = true;
      showFeedback("Config. H  ");
    } else if (estado == ESTADO_AJUSTE_HORA) {
      // B1 no ajuste de hora avanca para ajuste de minuto.
      estado = ESTADO_AJUSTE_MINUTO;
      showFeedback("Config. M  ");
      piscaUltimaTrocaMs = millis();
      piscaVisivel = true;
    } else if (estado == ESTADO_AJUSTE_MINUTO) {
      // Converte hora/minuto editados para segundos totais do dia.
      segundosRelogio = (uint32_t)(horaEdicao % 24) * 3600u + (uint32_t)(minutoEdicao % 60) * 60u;
      estado = ESTADO_RUN;
      pausado = false;
      ultimoTickSegundoMs = millis();
      showFeedback("Salvou! 0s");
    } else if (estado == ESTADO_CRONO) {
      // B1 no cronometro volta para o relogio normal.
      estado = ESTADO_RUN;
      cronoRodando = false;
      cronoAcumuladoMs = 0;
      pausado = false;
      ultimoTickSegundoMs = millis();
      showFeedback("Fim  cron. ");
    }
  } else if (indiceBotao == 1) {
    // B2: incrementa no ajuste e alterna iniciar/pausar no cronometro.
    if (estado == ESTADO_AJUSTE_HORA) {
      horaEdicao = (horaEdicao + 1) % 24;
      showFeedback("+1 hora   ");
      piscaUltimaTrocaMs = millis();
      piscaVisivel = true;
    } else if (estado == ESTADO_AJUSTE_MINUTO) {
      minutoEdicao = (minutoEdicao + 1) % 60;
      showFeedback("+1 min.   ");
      piscaUltimaTrocaMs = millis();
      piscaVisivel = true;
    } else if (estado == ESTADO_CRONO) {
      if (!cronoRodando) {
        // Retomada: guarda o instante de inicio da nova "perna".
        cronoInicioMs = millis();
        cronoRodando = true;
        showFeedback("> Corre   ");
      } else {
        // Pausa: soma o trecho corrido ao acumulado.
        cronoAcumuladoMs += millis() - cronoInicioMs;
        cronoRodando = false;
        showFeedback(">Pausa   ");
      }
    }
  } else {
    // B3: decrementa no ajuste e zera no cronometro.
    if (estado == ESTADO_AJUSTE_HORA) {
      horaEdicao = (horaEdicao + 23) % 24;
      showFeedback("-1 hora   ");
      piscaUltimaTrocaMs = millis();
      piscaVisivel = true;
    } else if (estado == ESTADO_AJUSTE_MINUTO) {
      minutoEdicao = (minutoEdicao + 59) % 60;
      showFeedback("-1 min.   ");
      piscaUltimaTrocaMs = millis();
      piscaVisivel = true;
    } else if (estado == ESTADO_CRONO) {
      if (cronoRodando) {
        // Se estava correndo, fecha primeiro o trecho atual.
        cronoAcumuladoMs += millis() - cronoInicioMs;
      }
      // Zera tudo e prepara para uma nova contagem.
      cronoAcumuladoMs = 0;
      cronoRodando = false;
      cronoInicioMs = millis();
      showFeedback("> Zero!   ");
    }
  }
}

// Desenha a interface no LCD conforme o estado atual do sistema.
void renderDisplay() {
  uint32_t agora = millis();

  // No cronometro, limita o redesenho para reduzir flicker e custo de LCD.
  if (estado == ESTADO_CRONO) {
    if (agora - redesenhoCronoMs < TEMPO_REDESENHO_CRONO_MS) {
      return;
    }
    redesenhoCronoMs = agora;
  } else {
    redesenhoCronoMs = agora;
  }

  // Nos modos de ajuste, alterna visibilidade para "piscar" o valor editado.
  if (estado == ESTADO_AJUSTE_HORA || estado == ESTADO_AJUSTE_MINUTO) {
    updateBlink();
  }

  if (estado == ESTADO_BOOT) {
    if (millis() < bootAteMs) {
      lcd.setCursor(0, 0);
      lcd.print("  ESP32  Rel.  ");
      lcd.setCursor(0, 1);
      lcd.print("  Inicializ... ");
      return;
    }

    // Quando o tempo de boot termina, entra no modo principal automaticamente.
    estado = ESTADO_RUN;
    pausado = false;
    ultimoTickSegundoMs = millis();
  }

  if (estado == ESTADO_RUN) {
    // segundosRelogio guarda segundos desde 00:00:00; aqui convertemos para HH:MM:SS.
    int hora = (segundosRelogio / 3600) % 24;
    int minuto = (segundosRelogio / 60) % 60;
    int segundo = (int)(segundosRelogio % 60);

    lcd.setCursor(0, 0);
    lcd.print("  ");
    print2Digits(hora);
    lcd.print(':');
    print2Digits(minuto);
    lcd.print(':');
    print2Digits(segundo);
    lcd.print("  ");

    lcd.setCursor(0, 1);
    if (millis() < feedbackAteMs) {
      printFeedbackLine();
    } else {
      lcd.print("EXEC B1+ B2^1.2s");
    }
  } else if (estado == ESTADO_AJUSTE_HORA) {
    lcd.setCursor(0, 0);
    lcd.print("MODO: CONFIG. ");
    lcd.setCursor(0, 1);

    if (millis() < feedbackAteMs) {
      printFeedbackLine();
    } else if (piscaVisivel) {
      lcd.print("Hora >>");
      print2Digits(horaEdicao);
      lcd.print("<<  ");
    } else {
      lcd.print("H. (pisca)  ");
    }
  } else if (estado == ESTADO_AJUSTE_MINUTO) {
    lcd.setCursor(0, 0);
    lcd.print("MODO: CONFIG. ");
    lcd.setCursor(0, 1);

    if (millis() < feedbackAteMs) {
      printFeedbackLine();
    } else if (piscaVisivel) {
      lcd.print("Min. >>");
      print2Digits(minutoEdicao);
      lcd.print("<<  ");
    } else {
      lcd.print("M. (pisca)  ");
    }
  } else {
    uint32_t tempoDecorrido = chronoMillis();
    // Formato exibido: MM:SS.D (decimos de segundo).
    uint32_t centesimos = tempoDecorrido / 10;
    uint32_t segundos = centesimos / 100;
    uint32_t minutos = (segundos / 60) % 100;
    uint32_t segundosExibicao = segundos % 60;
    int decimo = (int)(centesimos % 10);

    lcd.setCursor(0, 0);
    lcd.print("CRO ");
    print2Digits((int)minutos);
    lcd.print(':');
    print2Digits((int)segundosExibicao);
    lcd.print('.');
    print1Digit(decimo);
    lcd.print("   ");

    lcd.setCursor(0, 1);
    if (millis() < feedbackAteMs) {
      printFeedbackLine();
    } else {
      lcd.print("1=fim2=I/P3=0");
    }
  }
}

// Configura os pinos, o LCD e a tela inicial.
void setup() {
  initButtons();

  // Botoes com pull-up interno: sem resistor externo e leitura invertida.
  for (int indice = 0; indice < 3; indice++) {
    pinMode(PINOS_BOTOES[indice], INPUT_PULLUP);
  }

  pinMode(PINO_LED, OUTPUT);
  digitalWrite(PINO_LED, LOW);

  lcd.begin(16, 2);

  estado = ESTADO_BOOT;
  segundosRelogio = 0;
  ledAcesoAteMs = 0;
  redesenhoCronoMs = 0;
  bootAteMs = millis() + 850;
  ultimoTickSegundoMs = millis();

  lcd.setCursor(0, 0);
  lcd.print("  ESP32  Rel.  ");
  lcd.setCursor(0, 1);
  lcd.print("  Inicializ... ");
}

// O loop executa sempre na mesma ordem:
// 1) le botoes/eventos
// 2) atualiza relogio
// 3) atualiza LED
// 4) redesenha interface
// Essa ordem ajuda a manter resposta rapida e comportamento previsivel.
void loop() {
  bC();
  tickClock();
  updateLed();
  renderDisplay();
}
