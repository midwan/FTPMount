/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 * AmigaOS4 port (c) 2005-2006 Alexandre BALABAN (alexandre (at) free (dot) fr)
 */

#include	<stdio.h>

#include "FTPMount.h" /* 03-03-02 rri */
#include "site.h"
#include "strings.h"
#include "request.h"

#define V_IntuiText 18798
#define V_Gadget 18273

#define V_rastport 1001
#define V_bitmap 1008

struct RastPort *make_rastport(unsigned long width, unsigned long height, unsigned long depth, GraphicsParamDef)
{
	struct RastPort *rp;
	struct BitMap *bm;
	int i;
	unsigned char *z;

	rp = (struct RastPort *)allocate_flags(sizeof(*rp), MEMF_PUBLIC | MEMF_CLEAR, V_rastport);
	if (!rp) return 0;

	bm = (struct BitMap *)allocate_flags(sizeof(*bm), MEMF_PUBLIC | MEMF_CLEAR, V_bitmap);
	if (bm) {
		InitBitMap(bm, depth, width, height);

		bm->Planes[0] = AllocRaster(width, height * depth);
		if (bm->Planes[0]) {
			z = (unsigned char *)bm->Planes[0];
			for (i = 1; i < depth; i++) {
				z += bm->BytesPerRow * bm->Rows;
				bm->Planes[i] = (void *)z;
			}
			InitRastPort(rp);

			rp->BitMap = bm;
			rp->Mask = 0xff;

			return rp;
		}

		deallocate(bm, V_bitmap);
	}

	deallocate(rp, V_rastport);

	return 0;
}

void free_rastport(struct RastPort *rp, GraphicsParamDef)
{
	FreeRaster(rp->BitMap->Planes[0], rp->BitMap->BytesPerRow * 8, rp->BitMap->Rows * rp->BitMap->Depth);

	deallocate(rp->BitMap, V_bitmap);
	deallocate(rp, V_rastport);
}

struct IntuiText nulltext = {
   0, 0,
   JAM1,
   0, 0,
   (void *)0,
   "",
   (void *)0
};

struct gim *make_gim(unsigned char *name, unsigned long textpen, unsigned long lightpen, unsigned long darkpen, struct Screen *s,
	IntuitionParamDef, GraphicsParamDef)
{
	struct IntuiText txt;
	int width, height, owidth, oheight;
	struct gim *gim;
	signed long x, y, Rx, Ry, Rx2, Ry2, R2, new_delta, old_delta;

	txt.FrontPen = textpen;
	txt.BackPen = 0;
	txt.DrawMode = JAM2;
	txt.LeftEdge = 0;
	txt.TopEdge = 0;
	txt.ITextFont = s->Font;
	txt.IText = name;
	txt.NextText = nil;

	if (s->Font) height = s->Font->ta_YSize;
	else height = 8;

	width = IntuiTextLength(&txt);

	owidth = width + height;
	oheight = 3 * height / 2;

	gim = (struct gim *)allocate(sizeof(*gim), V_gim);
	if (!gim) return nil;

	ensure(gim, V_gim);

	gim->rp1 = make_rastport(owidth, oheight, 3, GraphicsParam);
	if (!gim->rp1) {
		deallocate(gim, V_gim);
		return nil;
	}

	gim->rp2 = make_rastport(owidth, oheight, 3, GraphicsParam);
	if (!gim->rp2) {
		free_rastport(gim->rp1, GraphicsParam);
		deallocate(gim, V_gim);
		return nil;
	}

	gim->im1.LeftEdge = 0;
	gim->im1.TopEdge = 0;
	gim->im1.Width = owidth;
	gim->im1.Height = oheight;
	gim->im1.Depth = 3;
	gim->im1.ImageData = (void *)gim->rp1->BitMap->Planes[0];

	gim->im1.PlanePick = 7;
	gim->im1.PlaneOnOff = 0;
	gim->im1.NextImage = nil;

	gim->im2 = gim->im1;
	gim->im2.ImageData = (void *)gim->rp2->BitMap->Planes[0];

	SetRast(gim->rp1, 0);
	SetRast(gim->rp2, 0);

	PrintIText(gim->rp1, &txt, height / 2, height / 4);
	PrintIText(gim->rp2, &txt, height / 2 + 1, height / 4 + 1);

	Rx = Ry = 2 * height / 3;

	Rx2 = Rx * Rx;
	Ry2 = Ry * Ry;

	R2 = Rx2 * Ry2;

	for (x = 0; x < Rx; x++) {
		y = 0;
		new_delta = abs(R2 - Ry2*x*x - Rx2*y*y);
		do {
			old_delta = new_delta;
			y++;
			new_delta = abs(R2 - Ry2*x*x - Rx2*y*y);
		} while (old_delta > new_delta);

		SetAPen(gim->rp1, lightpen);

		WritePixel(gim->rp1, Rx - x, Ry - (y - 1));
		WritePixel(gim->rp1, owidth - Rx - 1 + x, Ry - (y - 1));

		SetAPen(gim->rp1, darkpen);

		WritePixel(gim->rp1, Rx - x, oheight - Ry - 1 + (y - 1));
		WritePixel(gim->rp1, owidth - Rx - 1 + x, oheight - Ry - 1 + (y - 1));

		SetAPen(gim->rp2, darkpen);

		WritePixel(gim->rp2, Rx - x, Ry - (y - 1));
		WritePixel(gim->rp2, owidth - Rx - 1 + x, Ry - (y - 1));

		SetAPen(gim->rp2, lightpen);

		WritePixel(gim->rp2, Rx - x, oheight - Ry - 1 + (y - 1));
		WritePixel(gim->rp2, owidth - Rx - 1 + x, oheight - Ry - 1 + (y - 1));
	}

	for (y = 0; y < Ry; y++) {
		x = 0;
		new_delta = abs(R2 - Ry2 * x * x - Rx2 * y * y);
		do {
			old_delta = new_delta;
			x++;
			new_delta = abs(R2 - Ry2 * x * x - Rx2 * y * y);
		} while (old_delta > new_delta);

		SetAPen(gim->rp1, lightpen);

		WritePixel(gim->rp1, Rx - (x - 1), Ry - y);
		WritePixel(gim->rp1, Rx - (x - 1), oheight - Ry - 1 + y);

		SetAPen(gim->rp1, darkpen);

		WritePixel(gim->rp1, owidth - Rx - 1 + (x - 1), Ry - y);
		WritePixel(gim->rp1, owidth - Rx - 1 + (x - 1), oheight - Ry - 1 + y);

		SetAPen(gim->rp2, darkpen);

		WritePixel(gim->rp2, Rx - (x - 1), Ry - y);
		WritePixel(gim->rp2, Rx - (x - 1), oheight - Ry - 1 + y);

		SetAPen(gim->rp2, lightpen);

		WritePixel(gim->rp2, owidth - Rx - 1 + (x - 1), Ry - y);
		WritePixel(gim->rp2, owidth - Rx - 1 + (x - 1), oheight - Ry - 1 + y);
	}

	SetAPen(gim->rp1, lightpen);

	Move(gim->rp1, Rx, 0);
	Draw(gim->rp1, owidth - Rx - 1, 0);

	Move(gim->rp1, 0, Ry);
	Draw(gim->rp1, 0, oheight - Ry - 1);

	SetAPen(gim->rp1, darkpen);

	Move(gim->rp1, Rx, oheight - 1);
	Draw(gim->rp1, owidth - Rx - 1, oheight - 1);

	Move(gim->rp1, owidth - 1, Ry);
	Draw(gim->rp1, owidth - 1, oheight - Ry - 1);

	SetAPen(gim->rp2, darkpen);

	Move(gim->rp2, Rx, 0);
	Draw(gim->rp2, owidth - Rx - 1, 0);

	Move(gim->rp2, 0, Ry);
	Draw(gim->rp2, 0, oheight - Ry - 1);

	SetAPen(gim->rp2, lightpen);

	Move(gim->rp2, Rx, oheight - 1);
	Draw(gim->rp2, owidth - Rx - 1, oheight - 1);

	Move(gim->rp2, owidth - 1, Ry);
	Draw(gim->rp2, owidth - 1, oheight - Ry - 1);

	return gim;
}

void free_gim(struct gim *gim, IntuitionParamDef, GraphicsParamDef)
{
	verify(gim, V_gim);

	free_rastport(gim->rp1, GraphicsParam);
	free_rastport(gim->rp2, GraphicsParam);
	deallocate(gim, V_gim);

	return;
}

struct Gadget *make_gadget(struct gim *gim)
{
	struct Gadget *g;

	if (!gim) return nil;

	verify(gim, V_gim);

	g = (struct Gadget *)allocate(sizeof(*g), V_Gadget);
	if (!g) return nil;

	g->NextGadget = nil;
	g->LeftEdge = 0;
	g->TopEdge = 0;
	g->Width = gim->im1.Width;
	g->Height = gim->im1.Height;

	g->Flags = GFLG_GADGHIMAGE | GFLG_GADGIMAGE;
	g->Activation = GACT_RELVERIFY;
	g->GadgetType = GTYP_BOOLGADGET;
	g->GadgetRender = &gim->im1;
	g->SelectRender = &gim->im2;
	g->GadgetText = &nulltext;

	g->MutualExclude = 0;
	g->SpecialInfo = nil;
	g->GadgetID = 0;
	g->UserData = nil;

	return g;
}

void free_gadget(struct Gadget *g)
{
	deallocate(g, V_Gadget);
}

struct Window *connect_req(site *sp, unsigned char *s)
{
	struct Window *w;
#ifdef __amigaos4__
	struct IntuitionIFace * IIntuition;
	struct GraphicsIFace * IGraphics;
#else
	struct IntuitionBase *IntuitionBase;
	struct GfxBase *GfxBase;
#endif
	struct Screen *pub_screen;
	unsigned long screen_modeID;
	struct Rectangle rect;
	struct Gadget *cancel;
	struct IntuiText txt;
	int width, swidth, sheight, fheight;
	unsigned char *z;

	verify(sp, V_site);

#ifdef __amigaos4__
	IIntuition = sp->pIIntuition;
	IGraphics = sp->pIGraphics;
#else
	GfxBase = sp->GBase;
	IntuitionBase = sp->IBase;
#endif

	pub_screen = LockPubScreen(nil);
	if (pub_screen) {
		screen_modeID = GetVPModeID(&pub_screen->ViewPort);
		if (screen_modeID != INVALID_ID) {
			if (QueryOverscan(screen_modeID, &rect, OSCAN_TEXT)) {
				cancel = make_gadget(cancel_gim);
				if (cancel) {
					z = (unsigned char *)allocate(strlen(s) + 19, V_cstr);
					if (z) {
						strcpy(z, strings[MSG_CONNECTING_TO]);
						strcat(z, s);
						strcat(z, " ...");

						txt.FrontPen = 1;
						txt.BackPen = 0;
						txt.DrawMode = JAM1;
						txt.LeftEdge = 0;
						txt.TopEdge = 0;
						txt.ITextFont = pub_screen->Font;
						txt.IText = z;
						txt.NextText = nil;

						if (pub_screen->Font) fheight = pub_screen->Font->ta_YSize;
						else fheight = 8;

						width = IntuiTextLength(&txt) * 4 / 3;

						txt.LeftEdge = width / 8;
						txt.TopEdge = fheight / 2;

						swidth = rect.MaxX - rect.MinX + 1;
						sheight = rect.MaxY - rect.MinY + 1;

						if (pub_screen->TopEdge > 0)
							sheight -= pub_screen->TopEdge;

						if (sheight > pub_screen->Height) sheight = pub_screen->Height;
						if (swidth > pub_screen->Width) swidth = pub_screen->Width;

						if (sheight < fheight * 6) sheight = fheight * 6;
						if (swidth < width) swidth = width;

						w = OpenWindowTags(nil,
							WA_Left, swidth / 2 - pub_screen->LeftEdge - width / 2,
							WA_Top, sheight / 2 - pub_screen->TopEdge - fheight * 3,
							WA_Width, width,
							WA_Height, fheight * 6,
							WA_Flags, WFLG_DEPTHGADGET | WFLG_DRAGBAR |
							WFLG_SMART_REFRESH |
							WFLG_NOCAREREFRESH,
							WA_IDCMP, IDCMP_GADGETUP,
							WA_PubScreen, (ULONG)pub_screen,
							WA_Title, (ULONG)strings[MSG_CONNECTING],
							TAG_END, 0
							);

						if (w) {
							UnlockPubScreen(nil, pub_screen);
							PrintIText(w->RPort, &txt, 0, w->BorderTop);
							deallocate(z, V_cstr);
							w->UserData = (void *)cancel;
							cancel->LeftEdge = w->Width - w->BorderRight - fheight / 2 - cancel->Width;
							cancel->TopEdge = w->Height - w->BorderBottom - fheight / 3 - cancel->Height;
							AddGadget(w, cancel, (unsigned long)0);
							RefreshGList(cancel, w, nil, 1);

							return w;
						}

						deallocate(z, V_cstr);
					}

					free_gadget(cancel);
				}
			}
		}
		UnlockPubScreen(nil, pub_screen);
	}

	return nil;
}

void close_req(site *sp, struct Window *w)
{
	struct Gadget *cancel;
	struct Message *msg;

#ifdef __amigaos4__
	struct IntuitionIFace * IIntuition;
#else
	struct IntuitionBase *IntuitionBase;
#endif

	verify(sp, V_site);

#ifdef __amigaos4__
	IIntuition = sp->pIIntuition;
#else
	IntuitionBase = sp->IBase;
#endif

	Forbid();
	while (msg = GetMsg(w->UserPort)) ReplyMsg(msg);

	cancel = (struct Gadget *)w->UserData;
	RemoveGadget(w, cancel);

	free_gadget(cancel);

	CloseWindow(w);
	Permit();
}

struct phdata {
	unsigned char password[MAX_PASS_LENGTH + 1];
	unsigned char undo[MAX_PASS_LENGTH + 1];
};

#if defined(__MORPHOS__)
boolean password_hook(void)
#else
boolean SAVEDS ASM password_hook(REG(a0, struct Hook *hook),
	REG(a2, struct SGWork *sgw),
	REG(a1, unsigned long *msg))
#endif
{
#ifdef	__MORPHOS__
	struct Hook		*hook = (struct Hook *)REG_A0;
	unsigned long				*msg = (unsigned long *)REG_A1;
	struct SGWork	*sgw = (struct SGWork *)REG_A2;
#endif

	struct phdata *phd;

	phd = hook->h_Data;

	if (*msg == SGH_KEY) {
		switch (sgw->EditOp) {
		case EO_REPLACECHAR:
		case EO_INSERTCHAR:
			phd->password[sgw->BufferPos - 1] = sgw->WorkBuffer[sgw->BufferPos - 1];
			sgw->WorkBuffer[sgw->BufferPos - 1] = '*';
			break;
		case EO_MOVECURSOR:
			sgw->Actions &= ~SGA_USE;
			sgw->Actions |= SGA_BEEP;
			break;
		case EO_RESET:
			strcpy(phd->password, phd->undo);
			break;
		}
		if (sgw->BufferPos != sgw->NumChars) {
			sgw->BufferPos = sgw->NumChars;
			sgw->Actions |= SGA_REDISPLAY;
		}
		phd->password[sgw->NumChars] = 0;
		return true;
	}
	else if (*msg == SGH_CLICK) {
		sgw->BufferPos = sgw->NumChars;
		sgw->Actions |= SGA_REDISPLAY;
		return true;
	}
	else {
		return false;
	}
}

boolean user_pass_request(site *sp, struct Window *canw)
/*
 * open a requester asking for username and password, filled in as appropriate
 * Inputs:
 * sp : site pointer
 * canw  : cancel window (will still be open in the background, and may be used)
 *
 * Result:
 * false if cancel is selected in either canw or the requester, or a major failure occurred
 * true if login is selected, or return is pressed in password field
 */
{
#ifdef	__MORPHOS__
	struct EmulLibEntry	PassHookEmul;
#endif

	struct Gadget *glist, *gad, *login, *cancel, *userg, *passg;
	struct NewGadget user, pass;
	struct Screen *s;
	void *vi;
	unsigned long screen_modeID;
	struct Rectangle rect;
	struct Window *w;
	signed long swidth, sheight, fheight, wheight;
	unsigned long signals, csig;
	struct IntuiMessage *im;
	struct Hook pass_hook;
	unsigned char *z;
	struct phdata phd;

#ifdef __amigaos4__
	struct IntuitionIFace * IIntuition = sp->pIIntuition;
	struct GadToolsIFace * IGadTools = sp->pIGadTools;
	struct GraphicsIFace * IGraphics = sp->pIGraphics;
#else
	struct IntuitionBase *IntuitionBase;
	struct Library *GadToolsBase;
	struct GfxBase *GfxBase;
#endif

	verify(sp, V_site);

#ifdef __amigaos4__
	IIntuition = sp->pIIntuition;
	IGadTools = sp->pIGadTools;
	IGraphics = sp->pIGraphics;
#else
	IntuitionBase = sp->IBase;
	GfxBase = sp->GBase;
	GadToolsBase = sp->GTBase;
#endif

	glist = nil;

#ifndef	__MORPHOS__
	pass_hook.h_Entry = (unsigned long(*)())password_hook;
	pass_hook.h_SubEntry = (unsigned long(*)())nil;
	pass_hook.h_Data = (void *)&phd;
#else
	PassHookEmul.Trap = TRAP_LIB;
	PassHookEmul.Extension = 0;
	PassHookEmul.Func = (void(*)())password_hook;
	pass_hook.h_Entry = (unsigned long(*)())&PassHookEmul;
	pass_hook.h_SubEntry = (unsigned long(*)())nil;
	pass_hook.h_Data = (void *)&phd;
#endif

	s = LockPubScreen(nil);
	if (!s) return false;

	ScreenToFront(s);

	if (s->Font) fheight = s->Font->ta_YSize;
	else fheight = 8;

	if (sp->password) {
		strcpy(phd.password, sp->password);
		strcpy(phd.undo, sp->password);

		z = sp->password;
		while (*z) {
			*z++ = '*';
		}
	}
	else {
		phd.password[0] = 0;
		phd.undo[0] = 0;
	}

	wheight = fheight * 8;

	vi = GetVisualInfo(s, TAG_END);
	if (vi != nil) {
		screen_modeID = GetVPModeID(&s->ViewPort);
		if (screen_modeID != INVALID_ID) {
			if (QueryOverscan(screen_modeID, &rect, OSCAN_TEXT)) {
				swidth = rect.MaxX - rect.MinX + 1;
				sheight = rect.MaxY - rect.MinY + 1;

				sprintf(sp->read_buffer, strings[MSG_LOGIN_TO], sp->host);

				w = OpenWindowTags(nil,
					WA_Left, swidth / 4 - s->LeftEdge,
					WA_Top, sheight / 2 - wheight / 2 - s->TopEdge,
					WA_Width, swidth / 2,
					WA_Height, wheight,
					WA_IDCMP, IDCMP_ACTIVEWINDOW | IDCMP_REFRESHWINDOW |
					BUTTONIDCMP | STRINGIDCMP,
					WA_PubScreen, (ULONG)s,
					WA_Activate, true,
					WA_DragBar, true,
					WA_DepthGadget, true,
					WA_Title, (ULONG)sp->read_buffer,
					WA_AutoAdjust, true,
					TAG_END
					);
				if (w) {
					gad = CreateContext(&glist);
					if (gad) {
						user.ng_LeftEdge = w->Width / 4;
						user.ng_TopEdge = w->BorderTop + fheight / 2;
						user.ng_Width = (w->Width * 3 / 4) - w->BorderRight - fheight / 2;
						user.ng_Height = fheight * 3 / 2;
						user.ng_GadgetText = strings[MSG_USER_NAME];
						user.ng_TextAttr = s->Font;
						user.ng_GadgetID = 1;
						user.ng_Flags = PLACETEXT_LEFT;
						user.ng_VisualInfo = vi;

						pass.ng_LeftEdge = w->Width / 4;
						pass.ng_TopEdge = w->BorderTop + (fheight * 5 / 2);
						pass.ng_Width = (w->Width * 3 / 4) - w->BorderRight - fheight / 2;
						pass.ng_Height = fheight * 3 / 2;
						pass.ng_GadgetText = strings[MSG_PASSWORD_NAME];
						pass.ng_TextAttr = s->Font;
						pass.ng_GadgetID = 2;
						pass.ng_Flags = PLACETEXT_LEFT;
						pass.ng_VisualInfo = vi;

						userg = gad = CreateGadget(STRING_KIND, gad, &user,
							GTST_String, (ULONG)sp->user,
							GTST_MaxChars, MAX_USER_LENGTH,
							TAG_END
							);

						passg = gad = CreateGadget(STRING_KIND, gad, &pass,
							GTST_String, sp->password,
							GTST_MaxChars, MAX_PASS_LENGTH,
							GTST_EditHook, (unsigned long)&pass_hook,
							TAG_END
							);

						if (gad) {
							login = make_gadget(login_gim);
							if (login) {
								cancel = make_gadget(cancel_gim);
								if (cancel) {
									login->LeftEdge = fheight / 2 + w->BorderLeft;
									login->TopEdge = w->Height - w->BorderBottom - login->Height - fheight / 2;

									cancel->LeftEdge = w->Width - w->BorderRight - cancel->Width - fheight / 2;
									cancel->TopEdge = login->TopEdge;

									login->GadgetID = 3;
									cancel->GadgetID = 4;

									AddGadget(w, cancel, 100);
									AddGadget(w, login, 100);

									AddGList(w, glist, 0, 100, nil);

									RefreshGadgets(glist, w, nil);
									GT_RefreshWindow(w, nil);

									goto listen;
								}
								free_gadget(login);
							}
						}

						FreeGadgets(glist);
					}
					CloseWindow(w);
				}
			}
		}
		FreeVisualInfo(vi);
	}

	UnlockPubScreen(nil, s);

	return false;

listen:
	csig = 1 << canw->UserPort->mp_SigBit | sp->abort_signals | sp->disconnect_signals;
	signals = (1 << w->UserPort->mp_SigBit) | csig;

	while (1) {
		if (Wait(signals) & csig) {
			CloseWindow(w);

			FreeGadgets(glist);

			free_gadget(cancel);
			free_gadget(login);

			FreeVisualInfo(vi);
			UnlockPubScreen(nil, s);

			return false;
		}

		while (im = GT_GetIMsg(w->UserPort)) {
			gad = (struct Gadget *)im->IAddress;

			switch (im->Class) {
			case IDCMP_ACTIVEWINDOW:
				if (sp->needs_user) {
					ActivateGadget(userg, w, nil);
				}
				else {
					ActivateGadget(passg, w, nil);
				}
				break;
			case IDCMP_GADGETUP:
				switch (gad->GadgetID) {
				case 1:
					if (im->Code == 0) {
						ActivateGadget(passg, w, nil);
					}
					sp->needs_user = false;
					break;
				case 2:
					if (im->Code != 0) {
						break;
					}
					/* else fall through to Login */
				case 3:
					if (sp->user) deallocate(sp->user, V_cstr);
					if (sp->password) deallocate(sp->password, V_cstr);

					z = ((struct StringInfo *)userg->SpecialInfo)->Buffer;
					if (z[0] != '\0') {
						sp->user = (unsigned char *)allocate(strlen(z) + 1, V_cstr);
						if (sp->user) {
							strcpy(sp->user, z);
						}
					}
					else {
						sp->user = nil;
					}

					z = phd.password;
					if (z[0] != '\0') {
						sp->password = (unsigned char *)allocate(strlen(z) + 1, V_cstr);
						if (sp->password) {
							strcpy(sp->password, z);
						}
					}
					else {
						sp->password = nil;
					}

					Forbid();
					GT_ReplyIMsg(im);

					CloseWindow(w);
					Permit();

					FreeGadgets(glist);
					free_gadget(cancel);
					free_gadget(login);

					FreeVisualInfo(vi);
					UnlockPubScreen(nil, s);

					return true;
				case 4:
					Forbid();
					GT_ReplyIMsg(im);

					CloseWindow(w);
					Permit();

					FreeGadgets(glist);

					free_gadget(cancel);
					free_gadget(login);

					FreeVisualInfo(vi);
					UnlockPubScreen(nil, s);

					return false;
				}
				break;
			case IDCMP_REFRESHWINDOW:
				GT_BeginRefresh(w);
				GT_EndRefresh(w, true);
				break;
			}

			GT_ReplyIMsg(im);
		}
	}
}

struct Window *open_main_window(struct List *site_labels, IntuitionParamDef, GadToolsParamDef, GraphicsParamDef)
{
	struct Gadget *glist, *gad;
	struct NewGadget ng;
	struct Screen *s;
	void *vi;
	unsigned long screen_modeID;
	struct Rectangle rect;
	struct Window *w;
	signed long swidth, sheight;

	glist = nil;

	s = LockPubScreen(nil);
	if (!s) return nil;

	vi = GetVisualInfo(s, TAG_END);
	if (vi != nil) {
		screen_modeID = GetVPModeID(&s->ViewPort);
		if (screen_modeID != INVALID_ID) {
			if (QueryOverscan(screen_modeID, &rect, OSCAN_TEXT)) {
				swidth = rect.MaxX - rect.MinX + 1;
				sheight = rect.MaxY - rect.MinY + 1;

				w = OpenWindowTags(nil,
					WA_Left, swidth / 3 - s->LeftEdge,
					WA_Top, sheight / 4 - s->TopEdge,
					WA_Width, swidth / 3,
					WA_Height, sheight / 2,
					WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | LISTVIEWIDCMP,
					WA_PubScreen, s,
					WA_Activate, true,
					WA_DragBar, true,
					WA_DepthGadget, true,
					WA_CloseGadget, true,
					WA_Title, strings[MSG_CURRENT_SITES],
					WA_AutoAdjust, true,
					TAG_END
					);
				if (w) {
					gad = CreateContext(&glist);
					if (gad) {
						ng.ng_TextAttr = s->Font;
						ng.ng_VisualInfo = vi;
						ng.ng_LeftEdge = w->BorderLeft;
						ng.ng_TopEdge = w->BorderTop;
						ng.ng_Width = w->Width - w->BorderLeft - w->BorderRight;
						ng.ng_Height = w->Height - w->BorderTop - w->BorderBottom;
						ng.ng_GadgetText = nil;
						ng.ng_GadgetID = 0;
						ng.ng_Flags = 0;
						gad = CreateGadget(LISTVIEW_KIND, gad, &ng,
							GTLV_ReadOnly, false,
							GTLV_Labels, site_labels,
							TAG_END
							);
						if (gad) {
							AddGList(w, glist, 0, -1, nil);

							RefreshGadgets(glist, w, nil);
							GT_RefreshWindow(w, nil);

							w->UserData = (void *)glist;

							FreeVisualInfo(vi);
							UnlockPubScreen(nil, s);

							return w;
						}
						FreeGadgets(glist);
					}
					CloseWindow(w);
				}
			}
		}
		FreeVisualInfo(vi);
	}

	UnlockPubScreen(nil, s);

	return nil;
}

void close_main_window(struct Window *w, IntuitionParamDef, GadToolsParamDef)
{
	struct Gadget *glist;
	struct IntuiMessage *im;

	Forbid();
	while (im = GT_GetIMsg(w->UserPort)) {
		if (im->Class == IDCMP_REFRESHWINDOW) {
			GT_BeginRefresh(w);
			GT_EndRefresh(w, true);
		}
		GT_ReplyIMsg(im);
	}

	glist = (struct Gadget *)w->UserData;

	CloseWindow(w);
	FreeGadgets(glist);
	Permit();
}

#define V_List 19561
#define V_Node 20079

struct List *site_list(void)
{
	struct List *l;
	struct Node *n;
	site *sp;

	l = (struct List *)allocate(sizeof(*l), V_List);
	if (!l) return l;

	NewList(l);

	Forbid();
	sp = sites;
	while (sp) {
		n = (struct Node *)allocate(sizeof(*n), V_Node);
		if (!n) {
			Permit();
			return l;
		}

		n->ln_Name = (unsigned char *)allocate(strlen(sp->name) + 1, V_cstr);
		if (!n->ln_Name) {
			Permit();
			deallocate(n, V_Node);
			return l;
		}

		strcpy(n->ln_Name, sp->name);

		n->ln_Pri = 0;
		AddTail(l, n);
		sp = sp->next;
	}
	Permit();

	return l;
}

void free_labels(struct List *l)
{
	struct Node *n;

	while (n = RemHead(l)) {
		deallocate(n->ln_Name, V_cstr);
		deallocate(n, V_Node);
	}

	deallocate(l, V_List);
}

void draw_fill_bar(site *sp, IntuitionParamDef, GraphicsParamDef)
{
	struct Window *w;
	struct RastPort *rp;
	unsigned long x1, y1, x2, y2, gap;

	verify(sp, V_site);

	w = sp->status_window;
	if (!w) return;

	rp = w->RPort;

	x1 = w->BorderLeft;
	y1 = w->BorderTop;

	x2 = w->Width - w->BorderRight - 1;
	y2 = sp->abort_gadget->TopEdge - 1;

	gap = (y2 - y1) / 4;

	x2 = x1 + (x2 - x1) * 3 / 4;

	x1 += gap;
	y1 += gap;
	y2 -= gap;

	SetDrMd(rp, JAM2);

	SetAPen(rp, lightpen);
	Move(rp, x2, y1);
	Draw(rp, x2, y2);
	Draw(rp, x1, y2);
	SetAPen(rp, darkpen);
	Draw(rp, x1, y1);
	Draw(rp, x2, y1);

	SetAPen(rp, fillpen);

	x1++;
	y1++;
	y2--;
	x2--;

	if (sp->file_list) {
		verify(sp->file_list, V_file_info);

		x2 = x1 + ((x2 - x1) * sp->file_list->rpos) / sp->file_list->end;
		RectFill(rp, x1, y1, x2, y2);
	}
}

void update_fill_bar(site *sp, IntuitionParamDef, GraphicsParamDef)
{
	struct Window *w;
	struct RastPort *rp;
	unsigned long x1, y1, x2, y2, gap;
	struct IntuiText txt;
	unsigned char buffer[20], *z;
	file_info *fip;

	verify(sp, V_site);

	w = sp->status_window;
	if (!w) return;

	Forbid();
	fip = sp->file_list;
	if (!fip) {
		Permit();
		return;
	}
	verify(fip, V_file_info);

	z = fip->fname + strlen(fip->fname) - 1;
	while (z > fip->fname && *z != '/') z--;
	if (*z == '/') z++;

	rp = w->RPort;

	x1 = w->BorderLeft;
	y1 = w->BorderTop;

	x2 = w->Width - w->BorderRight - 1;
	y2 = sp->abort_gadget->TopEdge - 1;

	gap = (y2 - y1) / 4;

	x2 = x1 + (x2 - x1) * 3 / 4 - 1;

	x1 += gap + 1;
	y1 += gap + 1;
	y2 -= gap + 1;

	if (sp->site_state == SS_WRITING || !fip->end) {   /* just print how many k we have transferred */
		txt.FrontPen = textpen;
		txt.BackPen = 0;
		txt.DrawMode = JAM2;
		txt.LeftEdge = x1;
		txt.TopEdge = y1;
		txt.ITextFont = sp->status_window->WScreen->Font;
		txt.NextText = nil;
		txt.IText = z;
		PrintIText(rp, &txt, 0, 0);

		txt.LeftEdge += IntuiTextLength(&txt);
		txt.FrontPen = textpen;
		txt.IText = buffer;

		buffer[0] = ':';
		buffer[1] = ' ';
		x1 = 2;
		y1 = fip->rpos / 1024;
		buffer[x1] = '0';
		while (y1 >= 100000) {
			buffer[x1]++;
			y1 -= 100000;
		}
		if (buffer[x1] > '0') x1++;
		buffer[x1] = '0';
		while (y1 >= 10000) {
			buffer[x1]++;
			y1 -= 10000;
		}
		if (buffer[x1] > '0' || x1 != 2) x1++;
		buffer[x1] = '0';
		while (y1 >= 1000) {
			buffer[x1]++;
			y1 -= 1000;
		}
		if (buffer[x1] > '0' || x1 != 2) x1++;
		buffer[x1] = '0';
		while (y1 >= 100) {
			buffer[x1]++;
			y1 -= 100;
		}
		if (buffer[x1] > '0' || x1 != 2) x1++;
		buffer[x1] = '0';
		while (y1 >= 10) {
			buffer[x1]++;
			y1 -= 10;
		}
		if (buffer[x1] > '0' || x1 != 2) x1++;
		buffer[x1++] = y1 + '0';
		buffer[x1++] = ' ';
		buffer[x1++] = 'k';
		buffer[x1++] = ' ';
		buffer[x1++] = ' ';
		buffer[x1] = 0;

		PrintIText(rp, &txt, 0, 0);

		Permit();

		return;
	}

	/* reading ... can do a filler bar */

	SetDrMd(rp, JAM2);
	SetAPen(rp, fillpen);

	txt.FrontPen = lightpen;
	txt.BackPen = 0;
	txt.DrawMode = JAM1;
	txt.TopEdge = y1;
	txt.ITextFont = sp->status_window->WScreen->Font;
	txt.NextText = nil;
	txt.IText = z;

	txt.LeftEdge = (x1 + x2) / 2 - IntuiTextLength(&txt) / 2;

	x2 = x1 + ((x2 - x1) * sp->file_list->rpos) / sp->file_list->end;
	RectFill(rp, x1, y1, x2, y2);

	PrintIText(rp, &txt, 0, 0);

	Permit();

	return;
}

void draw_state(site *sp, IntuitionParamDef, GraphicsParamDef)
{
	struct IntuiText txt;

	/* gadget states */
	switch (sp->site_state) {
	case SS_DISCONNECTED:
	case SS_DISCONNECTING:
		if (!(sp->abort_gadget->Flags & GFLG_DISABLED)) {
			OffGadget(sp->abort_gadget, sp->status_window, nil);
		}
		if (!(sp->disconnect_gadget->Flags & GFLG_DISABLED)) {
			OffGadget(sp->disconnect_gadget, sp->status_window, nil);
		}
		break;
	case SS_CONNECTING:
	case SS_IDLE:
	case SS_LOGIN:
	case SS_ABORTING:
		if (!(sp->abort_gadget->Flags & GFLG_DISABLED)) {
			OffGadget(sp->abort_gadget, sp->status_window, nil);
		}
		if (sp->disconnect_gadget->Flags & GFLG_DISABLED) {
			OnGadget(sp->disconnect_gadget, sp->status_window, nil);
		}
		break;
	default:
		if (sp->abort_gadget->Flags & GFLG_DISABLED) {
			OnGadget(sp->abort_gadget, sp->status_window, nil);
		}
		if (sp->disconnect_gadget->Flags & GFLG_DISABLED) {
			OnGadget(sp->disconnect_gadget, sp->status_window, nil);
		}
		break;
	}

	txt.FrontPen = textpen;
	txt.BackPen = 0;
	txt.DrawMode = JAM2;
	txt.LeftEdge = 2;
	txt.TopEdge = 0;
	txt.ITextFont = sp->status_window->WScreen->Font;
	txt.NextText = nil;
	txt.IText = strings[MSG_STATE_UNKNOWN + sp->site_state];

	PrintIText(sp->status_window->RPort, &txt, sp->status_window->BorderLeft,
		sp->abort_gadget->TopEdge + sp->abort_gadget->Height / 4);

	if (sp->quick) {
		txt.LeftEdge += IntuiTextLength(&txt);
		txt.IText = strings[MSG_QUICK_FLAG];
		PrintIText(sp->status_window->RPort, &txt, sp->status_window->BorderLeft,
			sp->abort_gadget->TopEdge + sp->abort_gadget->Height / 4);
	}

	if (sp->site_state == SS_READING && sp->file_list && sp->file_list->end > 0) {
		draw_fill_bar(sp, IntuitionParam, GraphicsParam);
	}
}

void clear_state(site *sp, IntuitionParamDef, GraphicsParamDef)
{
	verify(sp, V_site);
	truth(sp->status_window != nil);

	SetAPen(sp->status_window->RPort, 0);
	SetDrMd(sp->status_window->RPort, JAM1);

	RectFill(sp->status_window->RPort, sp->status_window->BorderLeft, sp->status_window->BorderTop,
		sp->abort_gadget->LeftEdge - 1, sp->status_window->Height - sp->status_window->BorderBottom - 1);

	RectFill(sp->status_window->RPort, sp->abort_gadget->LeftEdge, sp->status_window->BorderTop,
		sp->status_window->Width - sp->status_window->BorderRight - 1,
		sp->abort_gadget->TopEdge - 1);
}

void open_status_window(site *sp, struct MsgPort *wport, IntuitionParamDef, GraphicsParamDef)
{
	struct Screen *s;
	struct Rectangle rect;
	unsigned long screen_modeID;
	struct Gadget *aborg, *disg;
	unsigned short swidth, sheight, fheight;
	struct Window *w;

	verify(sp, V_site);

	if (sp->status_window) {
		WindowToFront(sp->status_window);
		ActivateWindow(sp->status_window);
		return;
	}

	s = LockPubScreen(nil);
	if (!s) return;

	if (s->Font) fheight = s->Font->ta_YSize;
	else fheight = 8;

	screen_modeID = GetVPModeID(&s->ViewPort);
	if (screen_modeID != INVALID_ID) {
		if (QueryOverscan(screen_modeID, &rect, OSCAN_TEXT)) {
			aborg = make_gadget(abort_gim);
			if (aborg) {
				aborg->GadgetID = 1;
				disg = make_gadget(disconnect_gim);
				if (disg) {
					disg->GadgetID = 2;
					swidth = rect.MaxX - rect.MinX + 1;
					sheight = rect.MaxY - rect.MinY + 1;

					w = OpenWindowTags(nil,
						WA_Left, swidth / 4 - s->LeftEdge,
						WA_Top, sheight / 3 - s->TopEdge,
						WA_Width, swidth / 2,
						WA_Height, fheight * 4 + disg->Height,
						WA_Flags, WFLG_DEPTHGADGET | WFLG_DRAGBAR |
						WFLG_SMART_REFRESH |
						WFLG_CLOSEGADGET |
						WFLG_NOCAREREFRESH,
						WA_IDCMP, 0,
						WA_PubScreen, s,
						WA_Title, sp->name,
						TAG_END, 0
						);
					if (w) {
						UnlockPubScreen(nil, s);
						w->UserPort = wport;
						ModifyIDCMP(w, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP);

						aborg->NextGadget = disg;
						disg->NextGadget = nil;

						sp->abort_gadget = aborg;
						sp->disconnect_gadget = disg;

						disg->LeftEdge = w->Width - w->BorderRight - disg->Width - fheight / 2;
						disg->TopEdge = w->Height - w->BorderBottom - disg->Height - fheight / 3;

						aborg->TopEdge = disg->TopEdge;
						aborg->LeftEdge = disg->LeftEdge - aborg->Width - fheight / 2;

						AddGList(w, aborg, (unsigned long)-1, 2, nil);
						RefreshGadgets(aborg, w, nil);

						sp->status_window = w;
						w->UserData = (void *)sp;

						draw_state(sp, IntuitionParam, GraphicsParam);

						return;
					}
					free_gadget(disg);
				}
				free_gadget(aborg);
			}
		}
	}

	UnlockPubScreen(nil, s);
	return;
}

void close_status_window(site *sp, struct MsgPort *wport, IntuitionParamDef)
{
	struct IntuiMessage *im;
	struct Node *succ;
	struct Window *w;

	verify(sp, V_site);

	w = sp->status_window;

	if (!w) return;

	Forbid();
	im = (struct IntuiMessage *)wport->mp_MsgList.lh_Head;
	while (succ = im->ExecMessage.mn_Node.ln_Succ) {
		if (im->IDCMPWindow == w) {
			Remove((struct Node *)im);
			ReplyMsg((struct Message *)im);
		}
		im = (struct IntuiMessage *)succ;
	}

	w->UserPort = nil;
	ModifyIDCMP(w, 0);
	Permit();

	CloseWindow(w);

	free_gadget(sp->abort_gadget);
	free_gadget(sp->disconnect_gadget);

	sp->status_window = nil;

	return;
}

void SAVEDS status_handler(void)
{
	struct Process *me;
	struct MsgPort *rank, *sync, *myport, *cxport, *winport;
	status_message *reserve, *startup, *sm;
	struct Library *GadToolsBase, *CxBase;
	struct IntuitionBase *IntuitionBase;
	struct GfxBase *GfxBase;
#ifdef __amigaos4__
	struct GadToolsIFace * IGadTools = 0L;
	struct IntuitionIFace * IIntuition = 0L;
	struct GraphicsIFace * IGraphics = 0L;
	struct CommoditiesIFace * ICommodities = 0L; /* 2005-05-14 ABA : OS4 */
#endif
	CxObj *broker, *filter, *sender, *translate;
	CxMsg *cxmsg;
	unsigned long signals;
	unsigned long msgid, msgtype;
	struct Window *mainw;
	struct NewBroker nb;
	struct List *site_labels;
	struct IntuiMessage *imsg;
	struct Node *n;
	site *sp;

	mainw = nil;
	site_labels = nil;

	me = (struct Process *)FindTask(0l);
	myport = &me->pr_MsgPort;

	WaitPort(myport);
	startup = (status_message *)GetMsg(myport);

	mem_tracking_on();

	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
	if (IntuitionBase) {
#ifdef __amigaos4__
		IIntuition = (struct IntuitionIFace*)GetInterface((struct Library*)IntuitionBase, "main", 1L, 0);
		if (IIntuition) {
#endif
			GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
			if (GfxBase) {
#ifdef __amigaos4__
				IGraphics = (struct GraphicsIFace*)GetInterface((struct Library*)GfxBase, "main", 1L, 0);
				if (IGraphics) {
#endif
					GadToolsBase = OpenLibrary("gadtools.library", 0);
					if (GadToolsBase) {
#ifdef __amigaos4__
						IGadTools = (struct GadToolsIFace*)GetInterface((struct Library*)GadToolsBase, "main", 1L, 0);
						if (IGadTools) {
#endif
							CxBase = OpenLibrary("commodities.library", 0);
							if (CxBase) {
#ifdef __amigaos4__
								ICommodities = (struct CommoditiesIFace*)GetInterface((struct Library*)CxBase, "main", 1L, 0);
								if (ICommodities) {
#endif
									rank = CreatePort(0, 0);
									if (rank) {
										sync = CreatePort(0, 0);
										if (sync) {
											cxport = CreatePort(0, 0);
											if (cxport) {
												winport = CreatePort(0, 0);
												if (winport) {
													reserve = (status_message *)allocate_flags(sizeof(*reserve), MEMF_PUBLIC, V_status_message);
													if (reserve) {
														ensure(reserve, V_status_message);

														reserve->header.mn_Length = sizeof(*reserve);
														reserve->header.mn_ReplyPort = sync;
														reserve->header.mn_Node.ln_Name = "ftpstatus reserve message";
														reserve->header.mn_Node.ln_Type = NT_MESSAGE;
														reserve->header.mn_Node.ln_Pri = 0;

														nb.nb_Version = NB_VERSION;
														nb.nb_Name = strings[MSG_BROKER_NAME];
														nb.nb_Title = "FTPMount v" VERSION "." REVISION;
														nb.nb_Descr = strings[MSG_BROKER_DESCR];
														nb.nb_Unique = NBU_DUPLICATE;
														nb.nb_Flags = COF_SHOW_HIDE;
														nb.nb_Pri = 0;
														nb.nb_Port = cxport;
														nb.nb_ReservedChannel = 0;

														broker = CxBroker(&nb, nil);
														if (broker) {
															/* this hotkey stuff taken directly from RKM example */
															if (filter = CxFilter(strings[MSG_HOTKEY])) {
																/* if we can't add the hotkey stuff, don't fail */
																if (sender = CxSender(cxport, 1)) {
																	if (translate = (CxTranslate(nil))) {
																		AttachCxObj(broker, filter);
																		AttachCxObj(filter, sender);
																		AttachCxObj(filter, translate);
																	}
																}
															}
															ActivateCxObj(broker, 1l);

															startup->command = 0;
															ReplyMsg(&startup->header);
															goto begin_listening;
														}
														deallocate(reserve, V_status_message);
													}
													DeletePort(winport);
												}
												DeletePort(cxport);
											}
											DeletePort(sync);
										}
										DeletePort(rank);
									}
#ifdef __amigaos4__
									DropInterface((struct Interface*)ICommodities);
								}
#endif
								CloseLibrary(CxBase);
							}
#ifdef __amigaos4__
							DropInterface((struct Interface*)IGadTools);
						}
#endif
						CloseLibrary(GadToolsBase);
					}
#ifdef __amigaos4__
					DropInterface((struct Interface*)IGraphics);
				}
#endif
				CloseLibrary((struct Library *)GfxBase);
			}
#ifdef __amigaos4__
			DropInterface((struct Interface*)IIntuition);
		}
#endif
		CloseLibrary((struct Library *)IntuitionBase);
	}

	startup->command = SM_KILL;
	Forbid();
	ReplyMsg(&startup->header);
	return;

begin_listening:
	signals = (1 << myport->mp_SigBit) | (1 << cxport->mp_SigBit) | (1 << winport->mp_SigBit);

	while (1) {
		Wait(signals);

		while (cxmsg = (CxMsg *)GetMsg(cxport)) {
			msgid = CxMsgID(cxmsg);
			msgtype = CxMsgType(cxmsg);

			ReplyMsg((struct Message *)cxmsg);

			switch (msgtype) {
			case CXM_IEVENT:  /* copied from CXCMD_APPEAR below */
				if (mainw != nil) {
					close_main_window(mainw, IntuitionParam, GadToolsParam);
					mainw = nil;
					free_labels(site_labels);
				}

				site_labels = site_list();
				mainw = open_main_window(site_labels, IntuitionParam, GadToolsParam, GraphicsParam);
				signals = (1 << cxport->mp_SigBit) |
					(1 << myport->mp_SigBit) |
					(1 << winport->mp_SigBit);
				if (mainw != nil) {
					signals |= (1 << mainw->UserPort->mp_SigBit);
				}

				break;
			case CXM_COMMAND:
				switch (msgid) {
				case CXCMD_DISABLE:
					reserve->command = SM_SUSPEND;
					PutMsg(status_control, &reserve->header);
					WaitPort(sync); GetMsg(sync);

					ActivateCxObj(broker, 0l);
					break;
				case CXCMD_ENABLE:
					reserve->command = SM_RESUME;
					PutMsg(status_control, &reserve->header);
					WaitPort(sync); GetMsg(sync);

					ActivateCxObj(broker, 1l);
					break;
				case CXCMD_KILL:
					reserve->command = SM_KILL;
					PutMsg(status_control, &reserve->header);
					WaitPort(sync); GetMsg(sync);

					break;
				case CXCMD_APPEAR:
					if (mainw != nil) {
						close_main_window(mainw, IntuitionParam, GadToolsParam);
						mainw = nil;
						free_labels(site_labels);
					}

					site_labels = site_list();
					mainw = open_main_window(site_labels, IntuitionParam, GadToolsParam, GraphicsParam);
					signals = (1 << cxport->mp_SigBit) |
						(1 << myport->mp_SigBit) |
						(1 << winport->mp_SigBit);
					if (mainw != nil) {
						signals |= (1 << mainw->UserPort->mp_SigBit);
					}

					break;
				case CXCMD_DISAPPEAR:
					if (mainw != nil) {
						close_main_window(mainw, IntuitionParam, GadToolsParam);
						mainw = nil;
						free_labels(site_labels);

						signals = (1 << cxport->mp_SigBit) |
							(1 << myport->mp_SigBit) |
							(1 << winport->mp_SigBit);
					}
					break;
				}
			}
		}

		while (mainw && (imsg = GT_GetIMsg(mainw->UserPort))) {
			switch (imsg->Class) {
			case IDCMP_CLOSEWINDOW:
				GT_ReplyIMsg(imsg);
				close_main_window(mainw, IntuitionParam, GadToolsParam);
				mainw = nil;
				free_labels(site_labels);

				signals = (1 << cxport->mp_SigBit) |
					(1 << myport->mp_SigBit) |
					(1 << winport->mp_SigBit);
				continue;
			case IDCMP_REFRESHWINDOW:
				GT_BeginRefresh(mainw);
				GT_EndRefresh(mainw, true);
				break;
			case IDCMP_GADGETUP:
				msgid = imsg->Code;
				GT_ReplyIMsg(imsg);
				close_main_window(mainw, IntuitionParam, GadToolsParam);
				mainw = nil;

				n = site_labels->lh_Head;
				while (msgid-- && (n->ln_Succ)) {
					n = n->ln_Succ;
				}

				if (n->ln_Succ) {
					sp = sites;
					while (sp) {
						if (strcmp(sp->name, n->ln_Name) == 0) {
							break;
						}
						sp = sp->next;
					}

					if (sp) {
						open_status_window(sp, winport, IntuitionParam, GraphicsParam);
					}
				}

				free_labels(site_labels);

				signals = (1 << cxport->mp_SigBit) |
					(1 << myport->mp_SigBit) |
					(1 << winport->mp_SigBit);
				continue;
			default:
				break;
			}
			GT_ReplyIMsg(imsg);
		}

		while (imsg = (struct IntuiMessage *)GetMsg(winport)) {
			sp = (site *)imsg->IDCMPWindow->UserData;
			verify(sp, V_site);

			switch (imsg->Class) {
			case IDCMP_CLOSEWINDOW:
				ReplyMsg((struct Message *)imsg);
				close_status_window(sp, winport, IntuitionParam);
				continue;
			case IDCMP_GADGETUP:
				if (((struct Gadget *)imsg->IAddress)->GadgetID == 1) {
					/* abort */
					Signal(sp->port->mp_SigTask, sp->abort_signals);
				}
				else {
					/* disconnect */
					Signal(sp->port->mp_SigTask, sp->disconnect_signals);
				}
				break;
			}

			ReplyMsg((struct Message *)imsg);
		}

		while (sm = (status_message *)GetMsg(myport)) {
			verify(sm, V_status_message);

			switch (sm->command) {
			case SM_KILL:
				if (mainw != nil) {
					close_main_window(mainw, IntuitionParam, GadToolsParam);
					free_labels(site_labels);
				}

				Forbid();
				while (cxmsg = (CxMsg *)GetMsg(cxport)) {
					ReplyMsg((struct Message *)cxmsg);
				}

				DeleteCxObjAll(broker);
				Permit();

				deallocate(reserve, V_status_message);

				DeletePort(winport);
				DeletePort(cxport);
				DeletePort(sync);
				DeletePort(rank);

				check_memory();

#ifdef __amigaos4__
				DropInterface((struct Interface*)ICommodities);
				DropInterface((struct Interface*)IGadTools);
				DropInterface((struct Interface*)IGraphics);
				DropInterface((struct Interface*)IIntuition);
#endif

				CloseLibrary(CxBase);
				CloseLibrary(GadToolsBase);
				CloseLibrary((struct Library *)GfxBase);
				CloseLibrary((struct Library *)IntuitionBase);

				Forbid();
				ReplyMsg(&sm->header);
				return;
			case SM_NEW_SITE:
				if (sm->data) {
					open_status_window(sm->this_site, winport, IntuitionParam, GraphicsParam);
				}
				break;
			case SM_DEAD_SITE:
				close_status_window(sm->this_site, winport, IntuitionParam);
				break;
			case SM_STATE_CHANGE:
				if (sm->this_site->status_window) {
					clear_state(sm->this_site, IntuitionParam, GraphicsParam);
					draw_state(sm->this_site, IntuitionParam, GraphicsParam);
				}
				break;
			case SM_PROGRESS:
				if (sm->this_site->status_window) {
					update_fill_bar(sm->this_site, IntuitionParam, GraphicsParam);
				}
				break;
			}
			ReplyMsg(&sm->header);
		}
	}
}

