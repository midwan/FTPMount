/*
 * This source file is Copyright 1995 by Evan Scott.
 * All rights reserved.
 * Permission is granted to distribute this file provided no
 * fees beyond distribution costs are levied.
 */

#include "FTPMount.h" /* 03-03-02 rri */

#include "tcp.h"
#include "site.h"
#include "split.h"
#include "ftpinfo.h"
#include "connect.h"

/*
 * ftpinfo routines
 */

void add_ftpinfo(struct info_header *ih, b8 *name, b8 *comment, struct DateStamp ds,
   b32 size, b32 blocks, b32 flags)
{
   ftpinfo *fi;

   verify(ih, V_info_header);

   fi = allocate(sizeof(*fi) + strlen(name) + 1 + strlen(comment) + 1, V_ftpinfo);
   if (!fi)
      return;

   ensure(fi, V_ftpinfo);

   // Name, dann ein Nullbyte, dann den Kommentar speichern
   strcpy(fi->name, name);
   strcpy(fi->name + strlen(name) + 1, comment);

   fi->modified = ds;
   fi->size = size;
   fi->blocks = blocks;
   fi->flags = flags;

   // fi->next = ih->infos;
   // ih->infos = fi;

   /* instead of adding to front, and getting reversed lists, add to end */
   *(ih->last_info_p) = fi;
   ih->last_info_p = &(fi->next);
   fi->next = nil;

   return;
}

void free_info_header(struct info_header *ih)
{
   ftpinfo *fi, *fin;

   verify(ih, V_info_header);

   fi = ih->infos;
   while (fi) {
      verify(fi, V_ftpinfo);
      fin = fi->next;

      deallocate(fi, V_ftpinfo);
      fi = fin;
   }

   if (ih->next)
      ih->next->prev = ih->prev;

   *(ih->prev) = ih->next;

   deallocate(ih, V_info_header);

   return;
}

struct info_header *new_info_header(site *sp, b8 *name)
{
   struct info_header *ih;

   verify(sp, V_site);

   ih = (struct info_header *)allocate(sizeof(*ih) + strlen(name) + 1, V_info_header);
   if (!ih) return nil;

   ensure(ih, V_info_header);

   ih->infos = nil;
   ih->last_info_p = &(ih->infos);

   ih->next = sp->infos;
   if (sp->infos) sp->infos->prev = &(ih->next);
   sp->infos = ih;
   ih->prev = &(sp->infos);

   strcpy(ih->name, name);

   ih->case_sensitive = sp->case_sensitive;

   return ih;
}

struct info_header *find_info_header(site *sp, b8 *name)
{
   struct info_header *ih;

   verify(sp, V_site);

   ih = sp->infos;

   if (sp->case_sensitive) {
      while (ih) {
         if (strcmp(ih->name, name) == 0) return ih;
         ih = ih->next;
      }
   } else {
      while (ih) {
         if (stricmp(ih->name, name) == 0) return ih;
         ih = ih->next;
      }
   }

   return nil;
}

ftpinfo *find_info(struct info_header *ih, b8 *name)
{
   ftpinfo *fi, *found;

   found = nil;
   fi = ih->infos;

   if (ih->case_sensitive) {
      while (fi) {
         if (strcmp(fi->name, name) == 0 && !(fi->flags & MYFLAG_DELETED) )
         {
            found = fi;
            break;
         }
         fi = fi->next;
      }
   } else {
      found = nil;

      while (fi) {
         if (!(fi->flags & MYFLAG_DELETED))
         {
            if (stricmp(fi->name, name) == 0)
            {
               found = fi;

               if (strcmp(fi->name, name) == 0)
               {
                  break;
               }
            }
         }
         fi = fi->next;
      }
   }

   return found;
}

