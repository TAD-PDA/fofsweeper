#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KBLU "\x1B[34m"
#define KYEL "\x1B[33m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KNRM "\x1B[0m"
#define KCLS "\033[H\033[J"

#define HIHLIGHT_COMPLETE 1

int BOARDX;
int BOARDY;
int BOARDMINES;

struct termios old_tio, new_tio;

char my_getc(FILE *sin)
{
    char my_getc_temp_int = getc(sin);
    // printf("%d\n", my_getc_temp_int);
    return my_getc_temp_int;
}

enum field_pallete_index
{
    bombspace,
    emptyspace,
    onespace,
    twospace,
    threespace,
    fourspace,
    fivespace,
    sixspace,
    eightspace,
    coveredspace,
    explodespace,
    flagspace,
    linebreak
};
char field_pallete[] = {'X',
                        '.',
                        '1',
                        '2',
                        '3',
                        '4',
                        '5',
                        '6',
                        '7',
                        '8',
                        '+',
                        '*',
                        'P',
                        '\n'};

typedef struct
{
    signed char *data_field;
    char *data_bmask;
    char xsize;
    char ysize;
    char filled;
    int flags;
} t_mfield;

typedef struct
{
    char action;
    char xpos;
    char ypos;
    char xmax;
    char ymax;
} t_cursor;

char bombcheck_vectors_x[] = {-1, -1, -1, 0, 0, 1, 1, 1};
char bombcheck_vectors_y[] = {-1, 0, 1, -1, 1, -1, 0, 1};

t_cursor *cursor_init(char xmax, char ymax)
{
    t_cursor *new = calloc(1, sizeof(t_cursor));
    new->xmax = xmax;
    new->ymax = ymax;
    return new;
}

void cursor_delete(t_cursor *tbd)
{
    free(tbd);
}

t_mfield *mfield_init(char xsize, char ysize)
{
    t_mfield *new = malloc(sizeof(t_mfield));
    new->data_field = calloc(xsize * ysize, sizeof(char));
    new->data_bmask = calloc(xsize * ysize, sizeof(char));
    new->xsize = xsize;
    new->ysize = ysize;
    new->filled = 0;
    new->flags = 0;
    return new;
}

void mfield_delete(t_mfield *tbd)
{
    if (tbd)
    {
        free(tbd->data_bmask);
        free(tbd->data_field);
    }
    free(tbd);
}

void mfield_calculate(t_mfield *field)
{
    int i = 0;
    while (i < field->ysize)
    {
        int j = 0;
        while (j < field->xsize)
        {
            if (field->data_field[j + i * field->xsize] != -1)
            {
                int cellsum = 0;
                int ci = 0;
                while (ci < 8)
                {
                    if ((j + bombcheck_vectors_x[ci] >= 0) && (j + bombcheck_vectors_x[ci] < field->xsize) &&
                        (i + bombcheck_vectors_y[ci] >= 0) && (i + bombcheck_vectors_y[ci] < field->ysize))
                    {
                        cellsum += field->data_field[j + bombcheck_vectors_x[ci] + field->xsize * (i + bombcheck_vectors_y[ci])] == -1 ? 1 : 0;
                    }
                    ci++;
                }
                field->data_field[j + i * field->xsize] = cellsum;
            }
            j++;
        }
        i++;
    }
}

void mfield_fill(t_mfield *field, int n_mines, char safe_x, char safe_y)
{
    int mines_placed = 0;
    int x = 0, y = 0;
    while (mines_placed < n_mines &&
           mines_placed < ((field->xsize * field->ysize) - 9))
    {
        do
        {
            x = rand() % field->xsize;
            y = rand() % field->ysize;
        } while (field->data_field[x + field->xsize * y] != 0 || (((x == safe_x) && (y == safe_y)) ||
                                                                  ((x == safe_x + 1) && (y == safe_y)) ||
                                                                  ((x == safe_x - 1) && (y == safe_y)) ||
                                                                  ((x == safe_x + 1) && (y == safe_y - 1)) ||
                                                                  ((x == safe_x) && (y == safe_y - 1)) ||
                                                                  ((x == safe_x - 1) && (y == safe_y - 1)) ||
                                                                  ((x == safe_x + 1) && (y == safe_y + 1)) ||
                                                                  ((x == safe_x) && (y == safe_y + 1)) ||
                                                                  ((x == safe_x - 1) && (y == safe_y + 1))));
        printf("placed mine at %d %d\n", x, y);
        field->data_field[x + field->xsize * y] = -1;
        mines_placed++;
    }
    mfield_calculate(field);
    field->filled = 1;
};

void mfield_set_bytemask(t_mfield *field, char x, char y, char value)
{
    if (field->data_bmask[x + field->xsize * y] == 2)
    {
        if (value == 2)
        {
            field->flags--;
            field->data_bmask[x + field->xsize * y] = 0;
        }
        return;
    }
    if (field->data_bmask[x + field->xsize * y] == 1)
    {
        return;
    }
    field->flags += value - 1;
    field->data_bmask[x + field->xsize * y] = value;
}

void mfield_uncover(t_mfield *field, char x, char y)
{
    mfield_set_bytemask(field, x, y, 1);
    int i = 0;
    while (i < field->ysize)
    {
        int j = 0;
        while (j < field->xsize)
        {
            if (field->data_bmask[j + i * field->xsize] == 1 && field->data_field[j + i * field->xsize] == 0)
            {
                int cellsum = 0;
                int ci = 0;
                while (ci < 8)
                {
                    if ((j + bombcheck_vectors_x[ci] >= 0) && (j + bombcheck_vectors_x[ci] < field->xsize) &&
                        (i + bombcheck_vectors_y[ci] >= 0) && (i + bombcheck_vectors_y[ci] < field->ysize) &&
                        field->data_bmask[j + bombcheck_vectors_x[ci] + (i + bombcheck_vectors_y[ci]) * field->xsize] != 1)
                    {
                        mfield_uncover(field, j + bombcheck_vectors_x[ci], i + bombcheck_vectors_y[ci]);
                    }
                    ci++;
                }
            }
            j++;
        }
        i++;
    }
}

int mfield_check_cell_flags(t_mfield *field, char i, char j)
{
    if ((field->data_field[i + (j)*field->xsize] == 0) || (field->data_bmask[i + (j)*field->xsize] == 0))
        return 0;
    int cellsum = 0;
    int donearound = 0;
    int cellsaround = 0;
    int ci = 0;
    while (ci < 8)
    {
        if ((i + bombcheck_vectors_x[ci] >= 0) && (i + bombcheck_vectors_x[ci] < field->xsize) &&
            (j + bombcheck_vectors_y[ci] >= 0) && (j + bombcheck_vectors_y[ci] < field->ysize) &&
            field->data_bmask[i + bombcheck_vectors_x[ci] + (j + bombcheck_vectors_y[ci]) * field->xsize] != 1)
        {
            cellsum += field->data_bmask[i + bombcheck_vectors_x[ci] + (j + bombcheck_vectors_y[ci]) * field->xsize] == 2;
            donearound += field->data_bmask[i + bombcheck_vectors_x[ci] + (j + bombcheck_vectors_y[ci]) * field->xsize] != 0;
            cellsaround++;
        }
        ci++;
    }
    if ((cellsum == field->data_field[i + (j)*field->xsize]) && (donearound != cellsaround))
        return 1;
    return 0;
}

void mfield_click(t_mfield *field, char x, char y)
{
    if (field->data_bmask[x + y * field->xsize] == 1 && mfield_check_cell_flags(field, x, y))
    {

        int ci = 0;
        while (ci < 8)
        {
            if ((x + bombcheck_vectors_x[ci] >= 0) && (x + bombcheck_vectors_x[ci] < field->xsize) &&
                (y + bombcheck_vectors_y[ci] >= 0) && (y + bombcheck_vectors_y[ci] < field->ysize) &&
                field->data_bmask[x + bombcheck_vectors_x[ci] + (y + bombcheck_vectors_y[ci]) * field->xsize] != 1)
            {
                mfield_uncover(field, x + bombcheck_vectors_x[ci], y + bombcheck_vectors_y[ci]);
            }
            ci++;
        }
    }
    else
    {
        mfield_uncover(field, x, y);
    }
}

void mfield_draw_with_cursor(t_mfield *field, t_cursor *cur, FILE *sout)
{
    fputs(KCLS, sout);
    int i = 0;
    int j = 0;
    while (i < field->ysize)
    {
        while (j < field->xsize)
        {
            int complete = 0;
            if (complete = mfield_check_cell_flags(field, j, i))
            {
                if (cur == NULL || (cur->xpos == j && cur->ypos == i))
                {
                    fprintf(sout, KMAG);
                }
                else
                    fprintf(sout, KCYN);
            }
            else if (cur == NULL || (cur->xpos == j && cur->ypos == i))
            {
                fprintf(sout, KBLU);
            }
            else if (field->data_bmask[j + i * field->xsize] == 2)
            {
                fprintf(sout, KYEL);
            }
            fprintf(sout, "%c ", field->data_bmask[j + field->xsize * i] == 1 ? field_pallete[field->data_field[j + field->xsize * i] + 1] : field_pallete[field->data_bmask[j + field->xsize * i] + 10]);
            if (cur == NULL || (cur->xpos == j && cur->ypos == i) || complete || field->data_bmask[j + i * field->xsize] == 2)
            {
                fprintf(sout, KNRM);
            }
            j++;
        }
        j = 0;
        fputc(field_pallete[linebreak], sout);
        fputc(field_pallete[linebreak], sout);
        i++;
    }
    field->flags > BOARDMINES ? fprintf(sout, KRED) : 0;
    fprintf(sout, "FLAGS: %d/%d\n", field->flags, BOARDMINES);
    field->flags > BOARDMINES ? fprintf(sout, KNRM) : 0;
}

void mfield_draw_with_highlights(t_mfield *field, t_cursor *cur, FILE *sout, char win)
{
    fputs(KCLS, sout);
    int i = 0;
    int j = 0;
    while (i < field->ysize)
    {
        while (j < field->xsize)
        {
            if (field->data_field[j + field->xsize * i] == -1 || field->data_bmask[j + field->xsize * i] == 2)
            {
                if (!win)
                    fprintf(sout, KRED);
                else
                    fprintf(sout, KGRN);
            }
            if (field->data_field[j + field->xsize * i] == -1 && field->data_bmask[j + field->xsize * i] == 2)
            {
                fprintf(sout, KGRN);
            }
            fprintf(sout, "%c ", field->data_bmask[j + field->xsize * i] == 1 ? field_pallete[field->data_field[j + field->xsize * i] + 1] : field_pallete[field->data_bmask[j + field->xsize * i] + 10]);
            if (field->data_field[j + field->xsize * i] == -1 || field->data_bmask[j + field->xsize * i] == 2)
            {
                fprintf(sout, KNRM);
            }
            j++;
        }
        j = 0;
        fputc(field_pallete[linebreak], sout);
        fputc(field_pallete[linebreak], sout);
        i++;
    }
    field->flags > BOARDMINES ? fprintf(sout, KGRN) : 0;
    fprintf(sout, "FLAGS: %d/%d\n", field->flags, BOARDMINES);
    field->flags > BOARDMINES ? fprintf(sout, KNRM) : 0;
}

void cursor_read(t_cursor *cur, FILE *sin)
{
    char input = my_getc(sin);
    switch (input)
    {
    case 27:
        cur->action = 0;
        input = my_getc(sin);
        if (input == 91)
        {
            input = my_getc(sin);
            switch (input)
            {
            case 68:
                cur->xpos = (cur->xmax + cur->xpos - 1) % cur->xmax;
                break;
            case 67:
                cur->xpos = (cur->xmax + cur->xpos + 1) % cur->xmax;
                break;
            case 65:
                cur->ypos = (cur->ymax + cur->ypos - 1) % cur->ymax;
                break;
            case 66:
                cur->ypos = (cur->ymax + cur->ypos + 1) % cur->ymax;
                break;

            default:
                break;
            }
            // printf("%d %d\n", cur->xpos, cur->ypos);
        }
        break;
    default:
        cur->action = input;
        break;
    }
}

int cursor_perform(t_cursor *cur, t_mfield *field)
{
    switch (cur->action)
    {
    case '0':
        if (!field->filled)
        {
            mfield_fill(field, BOARDMINES, cur->xpos, cur->ypos);
        }
        mfield_click(field, cur->xpos, cur->ypos);
        break;
    case ' ':
        mfield_set_bytemask(field, cur->xpos, cur->ypos, 2);
        break;
    case EOF:
        return 1;
    default:
        break;
    }
    return 0;
}

int mfield_check_winlose(t_mfield *field)
{
    int correctmarks = 0;
    int correctopens = 0;
    int wrongmarks = 0;
    int i = 0;
    while (i < field->ysize)
    {
        int j = 0;
        while (j < field->xsize)
        {
            if (field->data_field[j + i * field->xsize] != -1 && field->data_bmask[j + i * field->xsize] == 1)
                correctopens++;
            if (field->data_field[j + i * field->xsize] == -1 && field->data_bmask[j + i * field->xsize] == 2)
                correctmarks++;
            if (field->data_field[j + i * field->xsize] == -1 && field->data_bmask[j + i * field->xsize] == 1)
                wrongmarks++;
            j++;
        }
        i++;
    }
    if (wrongmarks)
    {
        int i = 0;
        while (i < BOARDY)
        {
            int j = 0;
            while (j < BOARDX)
            {
                field->data_bmask[j + i * field->xsize] = field->data_bmask[j + i * field->xsize] == 2 ? 2 : 1;
                j++;
            }
            i++;
        }
        mfield_draw_with_highlights(field, NULL, stdout, 0);
        printf("YOU LOSE!\n");
        return 1;
    }
    if (correctmarks == BOARDMINES || correctopens == BOARDX * BOARDY - BOARDMINES)
    {
        int i = 0;
        while (i < BOARDY)
        {
            int j = 0;
            while (j < BOARDX)
            {
                field->data_bmask[j + i * field->xsize] = field->data_bmask[j + i * field->xsize] == 2 ? 2 : 1;
                j++;
            }
            i++;
        }
        mfield_draw_with_highlights(field, NULL, stdout, 1);
        printf("YOU WIN! \n");
        return 1;
    }
    return 0;
}

void term_init()
{
    unsigned char c;

    /* get the terminal settings for stdin */
    tcgetattr(STDIN_FILENO, &old_tio);

    /* we want to keep the old setting to restore them a the end */
    new_tio = old_tio;

    /* disable canonical mode (buffered i/o) and local echo */
    new_tio.c_lflag &= (~ICANON & ~ECHO);

    /* set the new settings immediately */
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
};

void term_close()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
};

int main()
{
    printf("Input: sizex, sizey, nmines:\n");
    scanf("%d %d %d", &BOARDX, &BOARDY, &BOARDMINES);

    BOARDMINES = BOARDMINES > BOARDX * BOARDY - 9 ? BOARDX * BOARDY - 9 : BOARDMINES;
    printf("Field: %dx%d, %d mines.\nPress the arrow keys to move, 0 to dig, space to mark.\nPress any key to continue.\n", BOARDX, BOARDY, BOARDMINES);
    getc(stdin);
    term_init();
    srand(time(NULL));
    t_mfield *new = mfield_init(BOARDX, BOARDY);
    t_cursor *new_c = cursor_init(BOARDX, BOARDY);
    // memset(new->data_bmask, 1, new->xsize *new->ysize);
    while (1)
    {
        cursor_read(new_c, stdin);
        if (cursor_perform(new_c, new))
            break;
        mfield_draw_with_cursor(new, new_c, stdout);
        if (mfield_check_winlose(new))
            break;
    }
    term_close();
}