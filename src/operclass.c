#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include "version.h"
#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

typedef struct _operClass_PathNode OperClass_PathNode;
typedef struct _operClass_CallbackNode OperClass_CallbackNode;

struct _operClass_PathNode
{
	OperClass_PathNode *prev,*next;
	OperClass_PathNode *children;
	char* identifier;
	OperClass_CallbackNode* callbacks;
};

struct _operClass_CallbackNode
{
	OperClass_CallbackNode *prev, *next;
	OperClassEntryEvalCallback callback;
};

OperClass_PathNode* rootEvalNode = NULL;

OperClassACLPath* OperClass_parsePath(char* path)
{
        OperClassACLPath* pathHead = NULL;
        OperClassACLPath* tmpPath;
        char *tmp = strdup(path);
        char *str = strtok(tmp,":");
        while (str)
        {
                tmpPath = MyMallocEx(sizeof(OperClassACLPath));
                tmpPath->identifier = str;
                AddListItem(tmpPath,pathHead);
        }

        return pathHead;
}

OperClassACL* OperClass_FindACL(OperClassACL* acl, char* name)
{
        for (;acl;acl = acl->next)
        {
                if (!strcmp(acl->name,name))
                { 
                        return acl;
                }
        }       
        return NULL;
}

OperClass_PathNode* OperClass_findPathNodeForIdentifier(char* identifier, OperClass_PathNode *head)
{
	for (; head; head = head->next)
	{
		if (!strcmp(head->identifier,identifier))
		{
			return head;
		}
	}
	return NULL;
}

unsigned char OperClass_evaluateACLEntry(OperClassACLEntry* entry, OperClassACLPath* path, OperClassCheckParams* params)
{
	OperClass_PathNode *node = rootEvalNode;	
	OperClass_CallbackNode *callbackNode = NULL;
	unsigned char eval = 0;

	/* Go as deep as possible */
	while (path->next && node)
	{
		node = OperClass_findPathNodeForIdentifier(path->identifier,node);	
		/* If we can't find a node we need, no match */
		if (!node)
		{
			return 0;
		}
		node = node->children;
	}

	/* If no evals for full path, no match */
	if (path->next)
	{
		return 0;
	}

	/* We have a valid node, execute all callback nodes */
	for (callbackNode = node->callbacks; callbackNode; callbackNode = callbackNode->next)
	{
		eval = callbackNode->callback(entry->variables,params);
	}

	return 0;	
}

OperPermission OperClass_evaluateACLPathEx(OperClassACL* acl, OperClassACLPath* path, OperClassCheckParams* params)
{
        /** Evaluate into ACL struct as deep as possible **/
	OperClassACLPath *basePath = path;
	path = path->next; /* Avoid first level since we have resolved it */
        OperClassACL* tmp;
        OperClassACLEntry* entry;
        unsigned char allow = 0;
        unsigned char deny = 0;
        while (path->next && acl->acls)
        {
                tmp = OperClass_FindACL(acl->acls,path->identifier);
                if (!tmp)
                {
                        break;
                }
                path = path->next;
                acl = tmp;
        }

        /** If node exists for this but has no ACL entries, allow **/
        if (!acl->entries)
        {
                return OPER_ALLOW;
        }

        /** Process entries **/
        for (entry = acl->entries; entry; entry = entry->next)
        {
                /* Short circuit if we already have valid block */
                if (entry->type == OPERCLASSENTRY_ALLOW && allow)
                        continue;
                if (entry->type == OPERCLASSENTRY_DENY && deny)
                        continue;

                unsigned char result = OperClass_evaluateACLEntry(entry,basePath,params);
                if (entry->type == OPERCLASSENTRY_ALLOW)
                {
                        allow = result;
                }
                deny = result;
        }

        /** We only permit if an allow matched AND no deny matched **/
        if (allow && !deny)
        {
                return OPER_ALLOW;
        }

        return OPER_DENY;
}

OperPermission OperClass_evaluateACLPath(char* operClass, char* path, OperClassCheckParams* params)
{
        ConfigItem_operclass *ce_operClass = Find_operclass(operClass);
        OperClass *oc = NULL;
        OperClassACLPath* operPath = OperClass_parsePath(path);
        OperClassACL* acl;
        if (ce_operClass)
        {
                oc = ce_operClass->classStruct;
        }

        while (oc && operPath)
        {
                OperClassACL* acl = OperClass_FindACL(oc->acls,operPath->identifier);
                if (acl)
                {
                        return OperClass_evaluateACLPathEx(oc->acls, operPath, params);
                }
                if (!oc->ISA)
                {
                        break;
                }
                ce_operClass = Find_operclass(oc->ISA);
		if (ce_operClass)
		{
			oc = ce_operClass->classStruct;
		}
        }

        return OPER_DENY;
}