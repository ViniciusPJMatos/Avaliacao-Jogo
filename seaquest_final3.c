#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
    static void dormir_ms(int ms) { Sleep(ms); }
    static void tocar_tom(int frequencia, int duracao_ms) {
        if (frequencia > 0 && duracao_ms > 0) {
            Beep(frequencia, duracao_ms);
        }
    }
    static int tecla_disponivel(void) { return _kbhit(); }
    static int ler_tecla(void) { return _getch(); }
    static void terminal_config_inicial(void) {}
#else
    #include <unistd.h>
    static void tocar_tom(int frequencia, int duracao_ms) {
        (void)frequencia;
        (void)duracao_ms;
    }
    #include <termios.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #include <time.h>

    static struct termios termios_original;

    static void terminal_restaurar(void) {
        tcsetattr(STDIN_FILENO, TCSANOW, &termios_original);
        fcntl(STDIN_FILENO, F_SETFL, 0);
    }

    static void terminal_config_inicial(void) {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &termios_original);
        raw = termios_original;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
        atexit(terminal_restaurar);
    }

    static int tecla_disponivel(void) {
        fd_set fds;
        struct timeval tv = {0, 0};
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
    }

    static int ler_tecla(void) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) == 1) return c;
        return -1;
    }

    static void dormir_ms(int ms) {
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
#endif

#define LARGURA        130
#define ALTURA         40
#define SUPERFICIE     1
#define MAX_TIROS      10
#define MAX_TIROS_INIMIGOS 8
#define MAX_INIMIGOS   14
#define MAX_MERGULH    4
#define OXIGENIO_MAX   100
#define VIDAS_INICIAIS 3
#define LINHAS_HUD     4
#define TOTAL_LINHAS   (ALTURA + 1 + LINHAS_HUD)
#define SPRITE_ALT_MAX 6


typedef struct {
    int largura;
    int altura;
    const char *linha[SPRITE_ALT_MAX];
} Sprite;

typedef enum {
    COR_RESET = 0,
    COR_AGUA,
    COR_FUNDO,
    COR_JOGADOR,
    COR_INIMIGO,
    COR_PEIXE,
    COR_MERGULHADOR,
    COR_TIRO,
    COR_HUD,
    COR_OXIGENIO
} CorTela;

static const char *codigo_cor(int cor) {
    switch (cor) {
        case COR_AGUA: return "\033[96m";      
        case COR_FUNDO: return "\033[94m";    
        case COR_JOGADOR: return "\033[92m";  
        case COR_INIMIGO: return "\033[91m";  
        case COR_PEIXE: return "\033[33m";   
        case COR_MERGULHADOR: return "\033[35m"; 
        case COR_TIRO: return "\033[97m";    
        case COR_HUD: return "\033[93m";     
        case COR_OXIGENIO: return "\033[36m"; 
        default: return "\033[0m";
    }
}

static const Sprite SPR_JOGADOR_DIR = {
    25, 4, {
        "               _|        ",
        "         ____| ~|_____   ",
        " ,_--~~~~         --  )  ",
        " `'~~~~~~~~~~~~~~~~~~~   "
        
    }
};
static const Sprite SPR_JOGADOR_ESQ = {
    25, 4, {
        "        |_               ",
        "   _____|~ |____         ",
        "  (  --         ~~~~--_, ",
        "   ~~~~~~~~~~~~~~~~~~~'` "
    }
};
#define JOGADOR_LARG 25
#define JOGADOR_ALT  4

static const Sprite SPR_SUB_DIR = {
    25, 4, {
        "               _|        ",
        "         ____| ~|_____   ",
        " ,_--~~~~         --  )  ",
        " `'~~~~~~~~~~~~~~~~~~~   "
    }
};
static const Sprite SPR_SUB_ESQ = {
    25, 4, {
        "        |_               ",
        "   _____|~ |____         ",
        "  (  --         ~~~~--_, ",
        "   ~~~~~~~~~~~~~~~~~~~'` "
    }
};
#define SUB_LARG 25
#define SUB_ALT  4

static const Sprite SPR_PEIXE_DIR = {
    22, 4, {
        "  \\.          |\\      ",
        "   \\`.___---~~  ~~~--_",
        "   //~~----___  (_o_-~",
        "  '           |/'     "
    }
};
static const Sprite SPR_PEIXE_ESQ = {
    22, 4, {
        "      /|          ./  ",
        "_--~~~  ~~---___.`/   ",
        "~-_o_)  ___----~~\\    ",
        "     '\\|           '  "
    }
};
#define PEIXE_LARG 22
#define PEIXE_ALT  4

static const Sprite SPR_MERGULHADOR = {
    11, 6, {
        "     (_>   ",
        "     T|_/  ",
        "     ||    ",
        "     -|    ",
        "     / \\   ",
        "    ==  == "
    }
};
#define MERG_LARG 11
#define MERG_ALT  6

static const Sprite SPR_TIRO_DIR = { 2, 1, { "->" } };
static const Sprite SPR_TIRO_ESQ = { 2, 1, { "<-" } };
#define TIRO_LARG 2
#define TIRO_ALT  1


typedef struct {
    int x, y;
    int ativo;
    int dir;
} Tiro;

typedef struct {
    int x, y;
    int ativo;
    int dir;
} TiroInimigo;

typedef struct {
    int x, y;
    int ativo;
    int dir;
    int tipo;       
    int velocidade;
    int contador;
} Inimigo;

typedef struct {
    int x, y;
    int ativo;
    int resgatado;
} Mergulhador;

typedef struct {
    int x, y;
    int dir;
    int oxigenio;
    int vidas;
    int pontos;
    int carregando;
} Jogador;

typedef enum {
    SOM_TIRO = 0,
    SOM_COLISAO,
    SOM_RESCATE,
    SOM_INIMIGO,
    SOM_GAME_OVER,
    SOM_NIVEL
} TipoSom;

static Jogador jogador;
static Tiro tiros[MAX_TIROS];
static TiroInimigo tiros_inimigos[MAX_TIROS_INIMIGOS];
static Inimigo inimigos[MAX_INIMIGOS];
static Mergulhador mergulhadores[MAX_MERGULH];
static char tela[TOTAL_LINHAS][LARGURA + 1];
static int cores[TOTAL_LINHAS][LARGURA];

static int nivel = 1;
static int game_over = 0;
static int tick = 0;
static int respawnando = 0;
static int respawn_timer = 0;

static void dimensoes_inimigo(int tipo, int *larg, int *alt) {
    if (tipo == 1) { *larg = SUB_LARG;  *alt = SUB_ALT;  }
    else           { *larg = PEIXE_LARG; *alt = PEIXE_ALT; }
}

static int sobrepoe(int x1, int y1, int w1, int h1,
                     int x2, int y2, int w2, int h2) {
    return (x1 < x2 + w2) && (x1 + w1 > x2) &&
           (y1 < y2 + h2) && (y1 + h1 > y2);
}


static void posicionar_inimigo_aleatorio(int i) {
    int larg, alt;
    dimensoes_inimigo(inimigos[i].tipo, &larg, &alt);
    int da_esquerda = 0, da_direita = 0;
    for (int j = 0; j < MAX_INIMIGOS; j++) {
        if (j == i || !inimigos[j].ativo) continue;
        if (inimigos[j].dir > 0) da_esquerda++;
        else da_direita++;
    }

    int lado;
    if (da_esquerda < da_direita) lado = 1;
    else if (da_direita < da_esquerda) lado = -1;
    else lado = (rand() % 2) ? 1 : -1;

    const int margem = 3;
    int y = SUPERFICIE + 2 + rand() % (ALTURA - alt - SUPERFICIE - 3);

    for (int tentativa = 0; tentativa < 100; tentativa++) {
        int muito_perto = 0;
        for (int j = 0; j < MAX_INIMIGOS; j++) {
            if (j == i || !inimigos[j].ativo || inimigos[j].dir != lado) continue;
            int larg_j, alt_j;
            dimensoes_inimigo(inimigos[j].tipo, &larg_j, &alt_j);
            (void)larg_j;
            if (abs(inimigos[j].y - y) < (alt + alt_j + margem)) {
                muito_perto = 1;
                break;
            }
        }
        if (!muito_perto) break;
        y = SUPERFICIE + 2 + rand() % (ALTURA - alt - SUPERFICIE - 3);
    }

    inimigos[i].y = y;
    inimigos[i].dir = lado;
    inimigos[i].x = (lado > 0) ? -larg : LARGURA;
}

static void posicionar_mergulhador_aleatorio(int i) {
    const int margem = 6; 
    for (int tentativa = 0; tentativa < 200; tentativa++) {
        int x = 1 + rand() % (LARGURA - MERG_LARG - 2);
        int y = SUPERFICIE + 5 + rand() % (ALTURA - MERG_ALT - SUPERFICIE - 6);

        if (sobrepoe(jogador.x, jogador.y, JOGADOR_LARG, JOGADOR_ALT,
                     x, y, MERG_LARG, MERG_ALT))
            continue;

        int muito_perto = 0;
        for (int j = 0; j < i; j++) {
            if (sobrepoe(x - margem, y - margem,
                         MERG_LARG + margem * 2, MERG_ALT + margem * 2,
                         mergulhadores[j].x, mergulhadores[j].y, MERG_LARG, MERG_ALT)) {
                muito_perto = 1;
                break;
            }
        }
        if (muito_perto) continue;

        mergulhadores[i].x = x;
        mergulhadores[i].y = y;
        return;
    }

    mergulhadores[i].x = 1 + rand() % (LARGURA - MERG_LARG - 2);
    mergulhadores[i].y = SUPERFICIE + 5 + rand() % (ALTURA - MERG_ALT - SUPERFICIE - 6);
}

static void reposicionar_mergulhadores(void) {
    for (int i = 0; i < MAX_MERGULH; i++) {
        mergulhadores[i].ativo = 1;
        mergulhadores[i].resgatado = 0;
        posicionar_mergulhador_aleatorio(i);
    }
}

static int quantidade_alvo_inimigos(void) {
    int alvo = 6 + (nivel - 1);
    if (alvo > MAX_INIMIGOS) alvo = MAX_INIMIGOS;
    return alvo;
}

static void reabastecer_inimigos(void) {
    int alvo = quantidade_alvo_inimigos();
    int vivos = 0;
    int submarinos = 0;

    for (int i = 0; i < MAX_INIMIGOS; i++) {
        if (inimigos[i].ativo) {
            vivos++;
            if (inimigos[i].tipo == 1) submarinos++;
        }
    }

    if (vivos >= alvo) return;

    int max_sub = (nivel >= 2) ? 1 + (nivel / 3) : 0;
    if (max_sub > 2) max_sub = 2;

    for (int i = 0; i < MAX_INIMIGOS && vivos < alvo; i++) {
        if (inimigos[i].ativo) continue;

        inimigos[i].ativo = 1;
        if (nivel >= 2 && submarinos < max_sub && (i == 0 || rand() % 3 == 0)) {
            inimigos[i].tipo = 1;
            submarinos++;
        } else {
            inimigos[i].tipo = 0;
        }
        int base = 2 - ((nivel - 1) / 3);
        if (base < 1) base = 1;
        inimigos[i].velocidade = base + (rand() % 2);
        inimigos[i].contador = 0;
        posicionar_inimigo_aleatorio(i);
        vivos++;
    }
}

static void inicializar_jogo(void) {
    jogador.x = LARGURA / 2;
    jogador.y = ALTURA / 2;
    jogador.dir = 1;
    jogador.oxigenio = OXIGENIO_MAX;
    jogador.vidas = VIDAS_INICIAIS;
    jogador.pontos = 0;
    jogador.carregando = 0;

    memset(tiros, 0, sizeof(tiros));
    memset(tiros_inimigos, 0, sizeof(tiros_inimigos));
    memset(inimigos, 0, sizeof(inimigos));
    memset(mergulhadores, 0, sizeof(mergulhadores));

    reabastecer_inimigos();

    for (int i = 0; i < MAX_MERGULH; i++) {
        mergulhadores[i].ativo = 1;
        mergulhadores[i].resgatado = 0;
        posicionar_mergulhador_aleatorio(i);
    }
}

static void resetar_jogo_mantendo_progresso(void) {
    int pontos = jogador.pontos;
    int vidas = jogador.vidas;
    if (vidas > VIDAS_INICIAIS) vidas = VIDAS_INICIAIS;
    inicializar_jogo();
    jogador.pontos = pontos;
    jogador.vidas = vidas;
}

static void tocar_som(TipoSom som) {
    switch (som) {
        case SOM_TIRO:
            tocar_tom(900, 50);
            break;
        case SOM_COLISAO:
            tocar_tom(220, 140);
            break;
        case SOM_RESCATE:
            tocar_tom(1320, 90);
            break;
        case SOM_INIMIGO:
            tocar_tom(680, 60);
            break;
        case SOM_GAME_OVER:
            tocar_tom(330, 120);
            dormir_ms(120);
            tocar_tom(250, 140);
            break;
        case SOM_NIVEL:
            tocar_tom(1040, 90);
            dormir_ms(80);
            tocar_tom(1320, 100);
            break;
        default:
            break;
    }
}

static void disparar_tiro(void) {
    for (int i = 0; i < MAX_TIROS; i++) {
        if (!tiros[i].ativo) {
            tiros[i].ativo = 1;
            tiros[i].dir = jogador.dir;
            tiros[i].y = jogador.y + 1;
            tocar_som(SOM_TIRO);
            tiros[i].x = (jogador.dir > 0)
                ? jogador.x + JOGADOR_LARG
                : jogador.x - TIRO_LARG;
            break;
        }
    }
}

static void respawnar_jogador(void) {
    int melhor_x = LARGURA / 2;
    int melhor_y = ALTURA / 2;
    int encontrou = 0;

    for (int y = SUPERFICIE; y <= ALTURA - JOGADOR_ALT; y++) {
        for (int x = 0; x <= LARGURA - JOGADOR_LARG; x++) {
            int seguro = 1;
            for (int j = 0; j < MAX_INIMIGOS; j++) {
                if (!inimigos[j].ativo) continue;
                int larg, alt;
                dimensoes_inimigo(inimigos[j].tipo, &larg, &alt);
                if (sobrepoe(x, y, JOGADOR_LARG, JOGADOR_ALT,
                             inimigos[j].x, inimigos[j].y, larg, alt)) {
                    seguro = 0;
                    break;
                }
            }
            if (seguro) {
                melhor_x = x;
                melhor_y = y;
                encontrou = 1;
                break;
            }
        }
        if (encontrou) break;
    }

    jogador.x = melhor_x;
    jogador.y = melhor_y;
    jogador.dir = 1;
}

static void perder_vida(void) {
    if (game_over || respawnando) return;

    if (jogador.vidas <= 0) {
        game_over = 1;
        return;
    }

    jogador.vidas--;
    if (jogador.vidas > 0) {
        jogador.oxigenio = OXIGENIO_MAX;
        respawnar_jogador();
        memset(tiros, 0, sizeof(tiros));
        memset(tiros_inimigos, 0, sizeof(tiros_inimigos));
        respawnando = 1;
        respawn_timer = 36;
        tocar_som(SOM_COLISAO);
        return;
    }

    jogador.vidas = 0;
    jogador.oxigenio = OXIGENIO_MAX;
    respawnar_jogador();
    memset(tiros, 0, sizeof(tiros));
    memset(tiros_inimigos, 0, sizeof(tiros_inimigos));
    respawnando = 1;
    respawn_timer = 60;
    tocar_som(SOM_COLISAO);
}

#define VELOCIDADE_JOGADOR 2

static void mover_jogador(int dx, int dy) {
    int nx = jogador.x + dx * VELOCIDADE_JOGADOR;
    int ny = jogador.y + dy * VELOCIDADE_JOGADOR;

    if (dx > 0) jogador.dir = 1;
    if (dx < 0) jogador.dir = -1;

    if (nx < 0) nx = 0;
    if (nx > LARGURA - JOGADOR_LARG) nx = LARGURA - JOGADOR_LARG;
    if (ny < SUPERFICIE) ny = SUPERFICIE;
    if (ny > ALTURA - JOGADOR_ALT) ny = ALTURA - JOGADOR_ALT;

    jogador.x = nx;
    jogador.y = ny;

    if (jogador.y == SUPERFICIE) {
        jogador.oxigenio = OXIGENIO_MAX;
        if (jogador.carregando == 4) {
            jogador.pontos += 200;
            reposicionar_mergulhadores();
            jogador.carregando = 0;
            nivel++;
            tocar_som(SOM_NIVEL);
            resetar_jogo_mantendo_progresso();
        }
    }
}

#define VELOCIDADE_TIRO_JOGADOR   4
#define VELOCIDADE_TIRO_INIMIGO   3

static void atualizar_tiros(void) {
    for (int i = 0; i < MAX_TIROS; i++) {
        if (tiros[i].ativo) {
            tiros[i].x += tiros[i].dir * VELOCIDADE_TIRO_JOGADOR;
            if (tiros[i].x < 0 || tiros[i].x > LARGURA - TIRO_LARG)
                tiros[i].ativo = 0;
        }
    }
}

static void atualizar_tiros_inimigos(void) {
    for (int i = 0; i < MAX_TIROS_INIMIGOS; i++) {
        if (tiros_inimigos[i].ativo) {
            tiros_inimigos[i].x += tiros_inimigos[i].dir * VELOCIDADE_TIRO_INIMIGO;
            if (tiros_inimigos[i].x < 0 || tiros_inimigos[i].x > LARGURA - TIRO_LARG)
                tiros_inimigos[i].ativo = 0;
            else if (!respawnando && sobrepoe(jogador.x, jogador.y, JOGADOR_LARG, JOGADOR_ALT,
                                              tiros_inimigos[i].x, tiros_inimigos[i].y, TIRO_LARG, TIRO_ALT)) {
                tiros_inimigos[i].ativo = 0;
                perder_vida();
            }
        }
    }
}

static void atualizar_inimigos(void) {
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        if (!inimigos[i].ativo) continue;

        int larg, alt;
        dimensoes_inimigo(inimigos[i].tipo, &larg, &alt);

        inimigos[i].contador++;
        if (inimigos[i].contador >= inimigos[i].velocidade) {
            inimigos[i].contador = 0;
            inimigos[i].x += inimigos[i].dir;
        }

        if (inimigos[i].x < -larg || inimigos[i].x > LARGURA) {
            inimigos[i].ativo = 0;
            continue;
        }

        if (!respawnando && sobrepoe(jogador.x, jogador.y, JOGADOR_LARG, JOGADOR_ALT,
                     inimigos[i].x, inimigos[i].y, larg, alt)) {
            perder_vida();
            inimigos[i].ativo = 0;
            continue;
        }

        int intervalo_tiro = 54 - (nivel - 1) * 3;
        if (intervalo_tiro < 24) intervalo_tiro = 24;

        if (inimigos[i].tipo == 1 && tick % intervalo_tiro == 0) {
            for (int t = 0; t < MAX_TIROS_INIMIGOS; t++) {
                if (!tiros_inimigos[t].ativo) {
                    tiros_inimigos[t].ativo = 1;
                    tiros_inimigos[t].dir = inimigos[i].dir;
                    tiros_inimigos[t].x = inimigos[i].x + (inimigos[i].dir > 0 ? SUB_LARG - 1 : 0);
                    tiros_inimigos[t].y = inimigos[i].y + 1;
                    break;
                }
            }
        }
    }
}

static void checar_colisoes_tiro(void) {
    for (int t = 0; t < MAX_TIROS; t++) {
        if (!tiros[t].ativo) continue;
        for (int i = 0; i < MAX_INIMIGOS; i++) {
            if (!inimigos[i].ativo) continue;
            int larg, alt;
            dimensoes_inimigo(inimigos[i].tipo, &larg, &alt);
            if (sobrepoe(tiros[t].x, tiros[t].y, TIRO_LARG, TIRO_ALT,
                         inimigos[i].x, inimigos[i].y, larg, alt)) {
                jogador.pontos += (inimigos[i].tipo == 1) ? 30 : 10;
                tocar_som(SOM_INIMIGO);
                inimigos[i].ativo = 0;
                tiros[t].ativo = 0;
                reabastecer_inimigos();
                break;
            }
        }
    }
}

static void checar_mergulhadores(void) {
    for (int i = 0; i < MAX_MERGULH; i++) {
        if (mergulhadores[i].ativo && !mergulhadores[i].resgatado &&
            jogador.carregando < 4 &&
            sobrepoe(jogador.x, jogador.y, JOGADOR_LARG, JOGADOR_ALT,
                     mergulhadores[i].x, mergulhadores[i].y, MERG_LARG, MERG_ALT)) {
            mergulhadores[i].resgatado = 1;
            mergulhadores[i].ativo = 0;
            jogador.carregando++;
            tocar_som(SOM_RESCATE);
        }
    }
}

static void atualizar_oxigenio(void) {
    if (!respawnando && tick % 21 == 0) {
        jogador.oxigenio--;
        if (jogador.oxigenio <= 0) {
            jogador.oxigenio = OXIGENIO_MAX / 2;
            perder_vida();
        }
    }
}

static void checar_vitoria_nivel(void) {
    (void)0;
}



static void carimbar_sprite(int x, int y, const Sprite *s, int cor) {
    for (int l = 0; l < s->altura; l++) {
        int ly = y + l;
        if (ly < 0 || ly >= TOTAL_LINHAS) continue;
        const char *linha = s->linha[l];
        for (int c = 0; c < s->largura; c++) {
            int lx = x + c;
            if (lx < 0 || lx >= LARGURA) continue;
            char ch = linha[c];
            if (ch != ' ') {
                tela[ly][lx] = ch;
                cores[ly][lx] = cor;
            }
        }
    }
}

static void montar_buffer(void) {
    for (int l = 0; l < TOTAL_LINHAS; l++) {
        memset(tela[l], ' ', LARGURA);
        tela[l][LARGURA] = '\0';
        for (int c = 0; c < LARGURA; c++) cores[l][c] = COR_RESET;
    }

    for (int x = 0; x < LARGURA; x++) {
        tela[SUPERFICIE - 1][x] = '~';
        cores[SUPERFICIE - 1][x] = COR_AGUA;
        tela[ALTURA][x] = '=';
        cores[ALTURA][x] = COR_FUNDO;
    }

    for (int i = 0; i < MAX_MERGULH; i++)
        if (mergulhadores[i].ativo && !mergulhadores[i].resgatado)
            carimbar_sprite(mergulhadores[i].x, mergulhadores[i].y, &SPR_MERGULHADOR, COR_MERGULHADOR);

    for (int i = 0; i < MAX_INIMIGOS; i++) {
        if (!inimigos[i].ativo) continue;
        const Sprite *s;
        if (inimigos[i].tipo == 1)
            s = (inimigos[i].dir > 0) ? &SPR_SUB_DIR : &SPR_SUB_ESQ;
        else
            s = (inimigos[i].dir > 0) ? &SPR_PEIXE_DIR : &SPR_PEIXE_ESQ;
        carimbar_sprite(inimigos[i].x, inimigos[i].y, s,
                        (inimigos[i].tipo == 1) ? COR_INIMIGO : COR_PEIXE);
    }

    for (int i = 0; i < MAX_TIROS; i++)
        if (tiros[i].ativo)
            carimbar_sprite(tiros[i].x, tiros[i].y,
                             tiros[i].dir > 0 ? &SPR_TIRO_DIR : &SPR_TIRO_ESQ,
                             COR_TIRO);

    if (respawnando) {
        const char *msg = (respawn_timer / 3) % 2 ? "VOCE MORREU!" : "RESPAWN...";
        int len = (int)strlen(msg);
        int x = (LARGURA - len) / 2;
        for (int i = 0; i < len; i++) {
            if (x + i >= 0 && x + i < LARGURA) {
                tela[2][x + i] = msg[i];
                cores[2][x + i] = COR_INIMIGO;
            }
        }
    }

    for (int i = 0; i < MAX_TIROS_INIMIGOS; i++)
        if (tiros_inimigos[i].ativo)
            carimbar_sprite(tiros_inimigos[i].x, tiros_inimigos[i].y,
                             tiros_inimigos[i].dir > 0 ? &SPR_TIRO_DIR : &SPR_TIRO_ESQ,
                             COR_INIMIGO);

    if (!respawnando || ((respawn_timer / 2) % 2 == 0)) {
        carimbar_sprite(jogador.x, jogador.y,
                         jogador.dir > 0 ? &SPR_JOGADOR_DIR : &SPR_JOGADOR_ESQ,
                         COR_JOGADOR);
    }

    int barras = (jogador.oxigenio * 20) / OXIGENIO_MAX;
    char barra_o2[21];
    for (int i = 0; i < 20; i++) barra_o2[i] = (i < barras) ? '#' : ' ';
    barra_o2[20] = '\0';

    snprintf(tela[ALTURA + 1], LARGURA + 1,
        "Nivel: %d  Pontos: %d  Vidas: %d  O2:[%s]%d%%",
        nivel, jogador.pontos, jogador.vidas, barra_o2, jogador.oxigenio);

    snprintf(tela[ALTURA + 2], LARGURA + 1,
        "Mergulhadores a bordo: %d/4 | +50/merg. | +200/fase%s",
        jogador.carregando,
        (jogador.oxigenio < OXIGENIO_MAX / 4) ? "   !!! OXIGENIO BAIXO - SUBA !!!" : "");

    snprintf(tela[ALTURA + 3], LARGURA + 1,
        "Pontos: +10 peixe | +30 sub | +50 resgate | WASD/ESPACO/Q");

    for (int c = 0; c < LARGURA; c++) {
        if (tela[ALTURA + 1][c] == '#') cores[ALTURA + 1][c] = COR_OXIGENIO;
        else if (tela[ALTURA + 1][c] != ' ') cores[ALTURA + 1][c] = COR_HUD;

        if (tela[ALTURA + 2][c] != ' ') cores[ALTURA + 2][c] = COR_HUD;
        if (tela[ALTURA + 3][c] != ' ') cores[ALTURA + 3][c] = COR_HUD;
    }
}

static void desenhar(void) {
    montar_buffer();
    printf("\033[H");
    for (int l = 0; l < TOTAL_LINHAS; l++) {
        int cor_atual = COR_RESET;
        for (int c = 0; c < LARGURA; c++) {
            int cor = cores[l][c];
            if (cor != cor_atual) {
                printf("%s", codigo_cor(cor));
                cor_atual = cor;
            }
            putchar(tela[l][c]);
        }
        printf("\033[0m\033[K\n");
    }
    fflush(stdout);
}

static int tela_game_over(void) {
    printf("\033[H\033[2J");
    printf("\n\n");
    printf("                 GAME OVER\n\n");
    printf("           Pontuacao final: %d\n", jogador.pontos);
    printf("           Nivel alcancado: %d\n\n", nivel);
    printf("      Pressione ENTER para jogar novamente\n");
    printf("           ou Q para sair...\n");
    fflush(stdout);

    while (1) {
        if (tecla_disponivel()) {
            int ch = ler_tecla();
            if (ch == 13 || ch == 'r' || ch == 'R') return 1;
            if (ch == 'q' || ch == 'Q') return 0;
        }
        dormir_ms(20);
    }
}


int main(void) {
    srand((unsigned int) time(NULL));

    terminal_config_inicial();
    printf("\033[?25l");
    printf("\033[2J");

    while (1) {
        game_over = 0;
        respawnando = 0;
        respawn_timer = 0;
        tick = 0;
        nivel = 1;
        inicializar_jogo();

        while (!game_over) {
            while (tecla_disponivel()) {
                int ch = ler_tecla();
                if (respawnando) {
                    if (ch == 'q' || ch == 'Q') game_over = 1;
                    continue;
                }
                switch (ch) {
                    case 'w': case 'W': mover_jogador(0, -1); break;
                    case 's': case 'S': mover_jogador(0, 1);  break;
                    case 'a': case 'A': mover_jogador(-1, 0); break;
                    case 'd': case 'D': mover_jogador(1, 0);  break;
                    case ' ':           disparar_tiro();       break;
                    case 'q': case 'Q': game_over = 1;         break;
                    default: break;
                }
            }

            atualizar_tiros();
            atualizar_tiros_inimigos();
            atualizar_inimigos();
            reabastecer_inimigos();
            checar_colisoes_tiro();
            checar_mergulhadores();
            atualizar_oxigenio();
            checar_vitoria_nivel();
            if (respawnando) {
                respawn_timer--;
                if (respawn_timer <= 0) respawnando = 0;
            }
            desenhar();

            tick++;
            dormir_ms(15);
        }

        tocar_som(SOM_GAME_OVER);
        if (!tela_game_over()) break;
    }

    printf("\033[?25h");
    fflush(stdout);
    return 0;
}
