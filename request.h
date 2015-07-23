/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

struct Window *connect_req(struct site_s *sp, b8 *);
void close_req(struct site_s *sp, struct Window *);
/*
ABA 25/05/2005
#ifdef __amigaos4__
struct gim *make_gim(b8 *name, b32 textpen, b32 lightpen, b32 darkpen, struct Screen *s,
			struct IntuitionBase *IntuitionBase, struct IntuitionIFace * IIntuition,
            struct GfxBase *GfxBase, struct GraphicsIFace * IGraphics);
void free_gim(struct gim *gim, struct IntuitionBase *IntuitionBase, struct IntuitionIFace * IIntuition,
            struct GfxBase *GfxBase, struct GraphicsIFace * IGraphics);
#else
struct gim *make_gim(b8 *name, b32 textpen, b32 lightpen, b32 darkpen, struct Screen *s,
			struct IntuitionBase *IntuitionBase, struct GfxBase *GfxBase);
void free_gim(struct gim *gim, struct IntuitionBase *IntuitionBase, struct GfxBase *GfxBase);
#endif*/
struct gim *make_gim(b8 *name, b32 textpen, b32 lightpen, b32 darkpen, struct Screen *s,
			IntuitionParamDef, GraphicsParamDef );
void free_gim(struct gim *gim, IntuitionParamDef, GraphicsParamDef);

boolean user_pass_request(struct site_s *sp, struct Window *canw);

void SAVEDS status_handler(void);

#define V_gim 99040

struct gim {
	magic_verify;

	struct Image im1;
	struct Image im2;
	struct RastPort *rp1;
	struct RastPort *rp2;
};

#define V_Gadget 18273

typedef struct status_message {
	struct Message header;
	
	magic_verify;
	
	b16 command;
	b16 data;
	struct site_s *this_site;
} status_message;

#define V_status_message 29549

#define SM_KILL 1
#define SM_SUSPEND 2
#define SM_RESUME 3
#define SM_NEW_SITE 4
#define SM_DEAD_SITE 5
#define SM_STATE_CHANGE 6
#define SM_PROGRESS 7

#define MAX_USER_LENGTH 40
#define MAX_PASS_LENGTH 40
