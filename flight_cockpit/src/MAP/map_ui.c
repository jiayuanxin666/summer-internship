#include "map_ui.h"
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static TTF_Font *open_font(int size)
{
    const char *paths[] = {"assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", "../assets/ALIBABAPUHUITI-2-45-LIGHT.TTF", "../../assets/ALIBABAPUHUITI-2-45-LIGHT.TTF"};
    for (unsigned i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) { TTF_Font *font = TTF_OpenFont(paths[i], size); if (font) return font; }
    return TTF_OpenFont("C:/Windows/Fonts/msyh.ttc", size);
}

static SDL_Texture *load_asset(SDL_Renderer *renderer, const char *name)
{
    const char *prefixes[] = {"assets/", "../assets/", "../../assets/"};
    char path[256];
    for (unsigned i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], name);
        SDL_Texture *texture = IMG_LoadTexture(renderer, path);
        if (texture) return texture;
    }
    return NULL;
}

static void text(MAP_UI *ui, TTF_Font *font, const char *value, int x, int y, SDL_Color color)
{
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, value, color); SDL_Texture *t; SDL_Rect r;
    if (!s) return;
    t = SDL_CreateTextureFromSurface(ui->renderer, s);
    r = (SDL_Rect){x,y,s->w,s->h};
    SDL_FreeSurface(s);
    if (t) { SDL_RenderCopy(ui->renderer, t, NULL, &r); SDL_DestroyTexture(t); }
}

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void panel(MAP_UI *ui, SDL_Rect box, const char *title, const char **labels, const char **values, int count)
{
    int head_height = clamp_int((int)(ui->height * .053f), 38, 52);
    int gap = clamp_int((int)(ui->height * .011f), 6, 12);
    int row_height = (box.h - head_height - gap * (count + 1)) / count;
    SDL_Color white={255,255,255,255}, black={25,32,39,255}; SDL_Rect head={box.x,box.y,box.w,head_height};
    SDL_SetRenderDrawBlendMode(ui->renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(ui->renderer,255,255,255,225); SDL_RenderFillRect(ui->renderer,&box);
    SDL_SetRenderDrawColor(ui->renderer,25,137,249,255); SDL_RenderFillRect(ui->renderer,&head); text(ui,ui->font_title,title,box.x+14,box.y+(head_height-28)/2,white);
    for(int i=0;i<count;i++){ SDL_Rect row={box.x+10,box.y+head_height+gap+i*(row_height+gap),box.w-20,row_height}; int baseline=row.y+(row.h-23)/2; SDL_SetRenderDrawColor(ui->renderer,189,195,199,255); SDL_RenderDrawRect(ui->renderer,&row); text(ui,ui->font,labels[i],row.x+8,baseline,(SDL_Color){85,92,99,255}); text(ui,ui->font,values[i],row.x+72,baseline,black); }
}

int MAP_UI_Init(MAP_UI *ui, int embedded)
{
    memset(ui,0,sizeof(*ui)); ui->width=1600; ui->height=900;
    ui->window=SDL_CreateWindow("Cabin Public Information - MAP",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1600,900,SDL_WINDOW_RESIZABLE|(embedded?SDL_WINDOW_BORDERLESS:0));
    if(!ui->window)return 0;
    ui->renderer=SDL_CreateRenderer(ui->window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ui->renderer)ui->renderer=SDL_CreateRenderer(ui->window,-1,SDL_RENDERER_SOFTWARE);
    ui->font=open_font(20); ui->font_title=open_font(24);
    ui->plane_texture=load_asset(ui->renderer,"plane.png");
    ui->add_texture=load_asset(ui->renderer,"add.png");
    ui->sub_texture=load_asset(ui->renderer,"sub.png");
    ui->fullscreen_texture=load_asset(ui->renderer,"full_screen.png");
    ui->window_texture=load_asset(ui->renderer,"window.png");
    return ui->renderer&&ui->font&&ui->font_title;
}

void MAP_UI_Destroy(MAP_UI *ui){SDL_Texture **textures[]={&ui->map_texture,&ui->plane_texture,&ui->add_texture,&ui->sub_texture,&ui->fullscreen_texture,&ui->window_texture};for(unsigned i=0;i<sizeof(textures)/sizeof(textures[0]);++i)if(*textures[i])SDL_DestroyTexture(*textures[i]);if(ui->font)TTF_CloseFont(ui->font);if(ui->font_title)TTF_CloseFont(ui->font_title);if(ui->renderer)SDL_DestroyRenderer(ui->renderer);if(ui->window)SDL_DestroyWindow(ui->window);memset(ui,0,sizeof(*ui));}

void MAP_UI_SetMapBytes(MAP_UI *ui,const unsigned char *bytes,size_t size){SDL_RWops *rw=SDL_RWFromConstMem(bytes,(int)size);SDL_Texture *next=rw?IMG_LoadTexture_RW(ui->renderer,rw,1):NULL;if(next){if(ui->map_texture)SDL_DestroyTexture(ui->map_texture);ui->map_texture=next;}}

static int hit(SDL_Rect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
void MAP_UI_HandleEvent(MAP_UI *ui,const SDL_Event *e,int *running,MAP_Data *data,int *refresh){if(e->type==SDL_QUIT)*running=0;else if(e->type==SDL_WINDOWEVENT&&e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED){ui->width=e->window.data1;ui->height=e->window.data2;}else if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_ESCAPE)*running=0;else if(e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_LEFT){int x=e->button.x,y=e->button.y;if(hit(ui->zoom_in_button,x,y)&&data->zoom<17){data->zoom++;*refresh=1;}else if(hit(ui->zoom_out_button,x,y)&&data->zoom>3){data->zoom--;*refresh=1;}else if(hit(ui->fullscreen_button,x,y)){ui->fullscreen=!ui->fullscreen;SDL_SetWindowFullscreen(ui->window,ui->fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);}else if(x<ui->width-(int)(ui->width*.18f)){ui->dragging_map=1;ui->drag_x=x;ui->drag_y=y;}}else if(e->type==SDL_MOUSEBUTTONUP&&e->button.button==SDL_BUTTON_LEFT&&ui->dragging_map){double scale=256.0*pow(2.0,data->zoom);int dx=e->button.x-ui->drag_x,dy=e->button.y-ui->drag_y;data->longitude-=dx/scale*360.0;double lat_rad=data->latitude*M_PI/180.0;data->latitude-=dy/scale*360.0*cos(lat_rad);if(data->latitude>85)data->latitude=85;if(data->latitude< -85)data->latitude=-85;ui->dragging_map=0;*refresh=1;}}

static void button(MAP_UI *ui,SDL_Rect r,const char *label){SDL_SetRenderDrawColor(ui->renderer,255,255,255,235);SDL_RenderFillRect(ui->renderer,&r);SDL_SetRenderDrawColor(ui->renderer,25,137,249,255);SDL_RenderDrawRect(ui->renderer,&r);int tw=0,th=0;TTF_SizeUTF8(ui->font_title,label,&tw,&th);text(ui,ui->font_title,label,r.x+(r.w-tw)/2,r.y+(r.h-th)/2,(SDL_Color){25,137,249,255});}

static void icon_button(MAP_UI *ui, SDL_Rect r, SDL_Texture *icon, const char *fallback)
{
    SDL_SetRenderDrawColor(ui->renderer,255,255,255,235);SDL_RenderFillRect(ui->renderer,&r);SDL_SetRenderDrawColor(ui->renderer,25,137,249,255);SDL_RenderDrawRect(ui->renderer,&r);
    if(icon){SDL_Rect dst={r.x+(r.w-40)/2,r.y+(r.h-40)/2,40,40};SDL_RenderCopy(ui->renderer,icon,NULL,&dst);}else button(ui,r,fallback);
}

void MAP_UI_Render(MAP_UI *ui,const MAP_Data *data)
{
    int margin=clamp_int((int)(ui->width*.008f),8,16);
    int side=clamp_int((int)(ui->width*.13f),208,320);
    int map_width=ui->width-side;
    int place_height=clamp_int((int)(ui->height*.25f),190,240);
    int weather_height=clamp_int((int)(ui->height*.37f),270,350);
    int panel_width=side-margin*2;
    SDL_Rect map={0,0,map_width,ui->height};
    SDL_SetRenderDrawColor(ui->renderer,19,31,43,255);SDL_RenderClear(ui->renderer);if(ui->map_texture)SDL_RenderCopy(ui->renderer,ui->map_texture,NULL,&map);
    else {text(ui,ui->font_title,"地图加载中...",map.w/2-70,map.h/2,(SDL_Color){220,230,238,255});}
    int cx=map.w/2,cy=map.h/2;if(ui->plane_texture){SDL_Rect plane={cx-20,cy-20,40,40};SDL_RenderCopyEx(ui->renderer,ui->plane_texture,NULL,&plane,data->bearing,NULL,SDL_FLIP_NONE);}else{double a=data->bearing*M_PI/180.0;SDL_Point p[4]={{cx+(int)(sin(a)*24),cy-(int)(cos(a)*24)},{cx+(int)(sin(a+2.5)*17),cy-(int)(cos(a+2.5)*17)},{cx-(int)(sin(a)*6),cy+(int)(cos(a)*6)},{cx+(int)(sin(a-2.5)*17),cy-(int)(cos(a-2.5)*17)}};SDL_SetRenderDrawColor(ui->renderer,255,112,31,255);SDL_RenderDrawLines(ui->renderer,p,4);SDL_RenderDrawLine(ui->renderer,p[3].x,p[3].y,p[0].x,p[0].y);}
    const char *place_labels[]={"城市","区县","街道"};const char *place_values[]={data->city,data->district,data->township};const char *weather_labels[]={"天气","温度","湿度","风力","风向"};char temp[32],hum[32],wind[32];snprintf(temp,sizeof(temp),"%s ℃",data->temperature);snprintf(hum,sizeof(hum),"%s %%",data->humidity);snprintf(wind,sizeof(wind),"%s 级",data->windpower);const char *weather_values[]={data->weather,temp,hum,wind,data->winddirection};
    panel(ui,(SDL_Rect){map.w+margin,margin*2,panel_width,place_height},"地点信息",place_labels,place_values,3);
    panel(ui,(SDL_Rect){map.w+margin,margin*3+place_height,panel_width,weather_height},"天气信息",weather_labels,weather_values,5);
    { int button_size=clamp_int((int)(ui->height*.064f),48,60); int y=ui->height-margin-button_size; int wide=panel_width-button_size*2-margin*2; if(wide<button_size)wide=button_size; ui->fullscreen_button=(SDL_Rect){map.w+margin,y,wide,button_size};ui->zoom_in_button=(SDL_Rect){ui->width-margin-button_size*2-margin,y,button_size,button_size};ui->zoom_out_button=(SDL_Rect){ui->width-margin-button_size,y,button_size,button_size}; }
    icon_button(ui,ui->fullscreen_button,ui->fullscreen?ui->window_texture:ui->fullscreen_texture,ui->fullscreen?"窗口":"全屏");icon_button(ui,ui->zoom_in_button,ui->add_texture,"+");icon_button(ui,ui->zoom_out_button,ui->sub_texture,"−");
    char coords[192];snprintf(coords,sizeof(coords),"%.4f, %.4f  Z%d  %.120s",data->longitude,data->latitude,data->zoom,data->status);text(ui,ui->font,coords,18,ui->height-36,(SDL_Color){255,255,255,255});SDL_RenderPresent(ui->renderer);
}
