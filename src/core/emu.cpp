#include "emu.h"

Emu::Emu() {
    memcpy(mem.data() + 0x50, font, 16 * 5);
    std::fill(display.begin(), display.end(), 0x6b38ff);
}

void Emu::load(std::string filename) {
    std::ifstream file(filename, std::ios::binary);
    if(!file.is_open()) {
        fprintf(stderr, "Couldn't open file %s\n", filename.c_str());
        exit(-1);
    }

    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    file.read((char*)(mem.data() + 0x200), size);
    file.close();
}

void Emu::execute(SDL_Event* evt) {
    u16 opcode = (mem[pc] << 8) | (mem[pc + 1]);
    u16 nnn = opcode & 0xfff;
    u8  n = opcode & 0xf;
    u8  x = (opcode & 0xf00) >> 8;
    u8  y = (opcode & 0xf0) >> 4;
    u8  kk = opcode & 0xff;
    pc += 2;

    switch(opcode & 0xf000) {
        case 0:
        _00kk(kk);
        break;
        case 0x1000: idle = (nnn == pc - 2); pc = nnn; break;
        case 0x2000: idle = (nnn == pc - 2); stack[sp] = pc; sp++; pc = nnn; break;
        case 0x3000: pc += (v[x] == kk) ? 2 : 0; break;
        case 0x4000: pc += (v[x] != kk) ? 2 : 0; break;
        case 0x5000: pc += (v[x] == v[y]) ? 2 : 0; break;
        case 0x6000: v[x] = kk; break;
        case 0x7000: v[x] += kk; break;
        case 0x8000:
        _8xyn(x,y,n);
        break;
        case 0x9000: pc += (v[x] != v[y]) ? 2 : 0; break;
        case 0xa000: I = nnn; break;
        case 0xb000: idle = (v[0] + nnn == pc - 2); pc = v[0] + nnn; break;
        case 0xc000: v[x] = (rand() % 255) & kk; break;
        case 0xd000: dxyn(x,y,n); break;
        case 0xe000:
        if(kk == 0x9e) { pc += (key[v[x]] == 1) ? 2 : 0; }
        else if (kk == 0xa1) { pc += (key[v[x]] == 0) ? 2 : 0; }
        else { fprintf(stderr, "Unimplemented instruction E%x%x\n", x, kk); exit(-1); }
        break;
        case 0xf000:
        fxkk(x, kk);
        break;
        default: fprintf(stderr,"Unimplemented instruction %x\n", opcode); exit(-1); break;
    }

    if (delay > 0)
        delay--;

    if (sound > 0)
        sound--;
}

void Emu::_00kk(u16 kk) {
    switch(kk) {
        case 0xe0:
        std::fill(display.begin(), display.end(), 0x6b38ff);
        break;
        case 0xee:
        sp--;
        idle = (stack[sp] == pc - 2);
        pc = stack[sp];
        break;
        default:
        fprintf(stderr, "Unimplemented instruction 00%x\n", kk);
        exit(-1);
    }
}

void Emu::_8xyn(u8 x, u8 y, u8 n) {
    switch(n) {
        case 0x0: v[x] = v[y]; break;
        case 0x1: v[x] |= v[y]; break;
        case 0x2: v[x] &= v[y]; break;
        case 0x3: v[x] ^= v[y]; break;
        case 0x4: {
            u16 result = v[x] + v[y];
            v[0xf] = (result > 0xff) ? 1 : 0;
            v[x] = result & 0xff;
        }
        break;
        case 0x5: v[0xf] = (v[x] > v[y]) ? 1 : 0; v[x] -= v[y]; break;
        case 0x6: v[0xf] = ((v[x] & 1) == 1) ? 1 : 0; v[x] *= 0.5; break;
        case 0x7: v[0xf] = (v[y] > v[x]) ? 1 : 0; v[x] = v[y] - v[x]; break;
        case 0xe: v[0xf] = ((v[x] & 0x80) == 0x80) ? 1 : 0; v[x] *= 2; break;
        default:
        fprintf(stderr, "Unimplemented instruction 8%x%x%x\n", x, y, n);
        exit(-1);
        break;
    }
}

void Emu::dxyn(u8 x, u8 y, u8 n) {
    v[0xf] = 0;
    for(int yy = 0; yy < n; yy++) {
        u8 data = mem[I+yy];
        for(int xx = 0; xx < 8; xx++) {
            if(data & (0x80 >> xx)) {
                u8 cx = (v[x] + xx) % 64, cy = (v[y] + yy) % 32;
                if(display[cx+64*cy] == 0x101820ff) {
                    v[0xf] = 1;
                    display[cx+64*cy] = 0x6b38ff;
                } else {
                    display[cx+64*cy] = 0x101820ff;
                }
                draw = true;
            }
        }
    }
}

void Emu::fxkk(u8 x, u16 kk) {
    switch(kk) {
        case 0x7: v[x] = delay; break;
        case 0xa:
        pc -= 2; idle = true; {int c = 0;
        for(auto i : key) {
            if(i) { v[x] = c; pc += 2; break; }
            c++;
        }}
        break;
        case 0x15: delay = v[x]; break;
        case 0x18: sound = v[x]; break;
        case 0x1e: I += v[x]; break;
        case 0x29: I = 0x50 + (5 * v[x]); break;
        case 0x33:
        mem[I + 2] = v[x] % 10;
        mem[I + 1] = v[x] / 10 % 10;
        mem[I] = v[x] / 100 % 10;
        break;
        case 0x55:
        for(int i = 0; i <= x; i++) {
            mem[I + i] = v[i];
        }
        break;
        case 0x65:
        for(int i = 0; i <= x; i++) {
            v[i] = mem[I + i];
        }
        break;
        default:
        fprintf(stderr, "Unimplemented instruction F%x%x\n", x, kk);
        exit(-1);
        break;
    }
}

void Emu::input(SDL_Event* evt, bool& quit) {
    while(SDL_PollEvent(evt)) {
        switch(evt->type) {
            case SDL_QUIT: quit = true; break;
            case SDL_KEYDOWN:
            switch(evt->key.keysym.sym) {
                case SDLK_1: key[0x1] = 1; break;
                case SDLK_2: key[0x2] = 1; break;
                case SDLK_3: key[0x3] = 1; break;
                case SDLK_4: key[0xc] = 1; break;
                case SDLK_q: key[0x4] = 1; break;
                case SDLK_w: key[0x5] = 1; break;
                case SDLK_e: key[0x6] = 1; break;
                case SDLK_r: key[0xd] = 1; break;
                case SDLK_a: key[0x7] = 1; break;
                case SDLK_s: key[0x8] = 1; break;
                case SDLK_d: key[0x9] = 1; break;
                case SDLK_f: key[0xe] = 1; break;
                case SDLK_z: key[0xa] = 1; break;
                case SDLK_x: key[0x0] = 1; break;
                case SDLK_c: key[0xb] = 1; break;
                case SDLK_v: key[0xf] = 1; break;
                case SDLK_h: reset(); break;
            }
            break;
            case SDL_KEYUP:
            switch(evt->key.keysym.sym) {
                case SDLK_1: key[0x1] = 0; break;
                case SDLK_2: key[0x2] = 0; break;
                case SDLK_3: key[0x3] = 0; break;
                case SDLK_4: key[0xc] = 0; break;
                case SDLK_q: key[0x4] = 0; break;
                case SDLK_w: key[0x5] = 0; break;
                case SDLK_e: key[0x6] = 0; break;
                case SDLK_r: key[0xd] = 0; break;
                case SDLK_a: key[0x7] = 0; break;
                case SDLK_s: key[0x8] = 0; break;
                case SDLK_d: key[0x9] = 0; break;
                case SDLK_f: key[0xe] = 0; break;
                case SDLK_z: key[0xa] = 0; break;
                case SDLK_x: key[0x0] = 0; break;
                case SDLK_c: key[0xb] = 0; break;
                case SDLK_v: key[0xf] = 0; break;
            }
            break;
        }
    }
}

void Emu::reset() {
    std::fill(display.end(), display.end(), 0x6b38ff);
    std::fill(key.begin(), key.end(), 0);
    std::fill(stack.begin(), stack.end(), 0);
    draw = false; idle = false;
    pc = 0x200; I = 0;
    std::fill(v.begin(), v.end(), 0);
    sp = 0; delay = 0; sound = 0;
}
