#include "npc_behaviortree.h"

#include "b_local.h"
#include "g_local.h"
#include "g_nav.h"

typedef enum btNodeType_e
{
	BT_NODE_TYPE_SELECTOR,
	BT_NODE_TYPE_SEQUENCE,
	BT_NODE_TYPE_DECORATOR,
	BT_NODE_TYPE_LEAF,
} btNodeType_t;

typedef struct btNode_s
{
	btNodeType_t type;
	btNodeState_t state;
} btNode_t;

typedef struct btSelectorNode_s
{
	btNodeType_t type;
	btNodeState_t state;

	int numChildren;
	int activeChildIndex;
	btNode_t **children;
} btSelectorNode_t;

typedef struct btSequenceNode_s
{
	btNodeType_t type;
	btNodeState_t state;

	int numChildren;
	int activeChildIndex;
	btNode_t **children;
} btSequenceNode_t;

typedef struct btDecoratorNode_s
{
	btNodeType_t type;
	btNodeState_t state;

	btNodeState_t(*execute)(behaviorTree_t *, btNode_t *);

	btNode_t *child;
	int value;
} btDecoratorNode_t;

typedef struct btLeafNode_s
{
	btNodeType_t type;
	btNodeState_t state;

	btNodeState_t(*execute)(behaviorTree_t *, btNode_t *);

	int value;
} btLeafNode_t;

typedef struct behaviorTree_s
{
	btNode_t *root;
} behaviorTree_t;

static behaviorTree_t s_behaviorTrees[MAX_GENTITIES];

btNode_t *NPC_BT_CreateSelectorNode(int numChildren, ...)
{
	btSelectorNode_t *node;
	trap->TrueMalloc(&node, sizeof(btSelectorNode_t));

	node->type = BT_NODE_TYPE_SELECTOR;
	node->state = BT_NODE_STATE_INVALID;
	node->numChildren = numChildren;
	node->activeChildIndex = 0;

	size_t arraySize = sizeof(btNode_t *) * numChildren;
	trap->TrueMalloc((void **)&node->children, arraySize);

	va_list args;
	va_start(args, numChildren);
	for (int i = 0; i < numChildren; ++i)
	{
		node->children[i] = va_arg(args, btNode_t *);
	}
	va_end(args);

	return (btNode_t *)node;
}

btNode_t *NPC_BT_CreateSequenceNode(int numChildren, ...)
{
	btSequenceNode_t *node;
	trap->TrueMalloc(&node, sizeof(btSequenceNode_t));

	node->type = BT_NODE_TYPE_SEQUENCE;
	node->state = BT_NODE_STATE_INVALID;
	node->numChildren = numChildren;
	node->activeChildIndex = 0;

	size_t arraySize = sizeof(btNode_t *) * numChildren;
	trap->TrueMalloc((void **)&node->children, arraySize);

	va_list args;
	va_start(args, numChildren);
	for (int i = 0; i < numChildren; ++i)
	{
		node->children[i] = va_arg(args, btNode_t *);
	}
	va_end(args);

	return (btNode_t *)node;
}

btNode_t *NPC_BT_CreateDecoratorNode(btNodeState_t(*fn)(behaviorTree_t *, btNode_t *), btNode_t *child)
{
	btDecoratorNode_t *node;
	trap->TrueMalloc(&node, sizeof(btDecoratorNode_t));

	node->type = BT_NODE_TYPE_DECORATOR;
	node->state = BT_NODE_STATE_INVALID;
	node->execute = fn;
	node->child = child;

	return (btNode_t *)node;
}

btNode_t *NPC_BT_CreateLeafNode(btNodeState_t(*fn)(behaviorTree_t *, btNode_t *))
{
	btLeafNode_t *node;
	trap->TrueMalloc(&node, sizeof(btLeafNode_t));

	node->type = BT_NODE_TYPE_LEAF;
	node->state = BT_NODE_STATE_INVALID;
	node->execute = fn;

	return (btNode_t *)node;
}

static void NPC_BT_FreeNode(btNode_t *btNode)
{
	switch (btNode->type)
	{
	case BT_NODE_TYPE_SELECTOR:
	{
		btSelectorNode_t *btSelectorNode = (btSelectorNode_t *)btNode;
		for (int i = 0; i < btSelectorNode->numChildren; ++i)
		{
			NPC_BT_FreeNode(btSelectorNode->children[i]);
		}
		trap->TrueFree((void **)&btSelectorNode->children);
		break;
	}

	case BT_NODE_TYPE_SEQUENCE:
	{
		btSequenceNode_t *btSequenceNode = (btSequenceNode_t *)btNode;
		for (int i = 0; i < btSequenceNode->numChildren; ++i)
		{
			NPC_BT_FreeNode(btSequenceNode->children[i]);
		}
		trap->TrueFree((void **)&btSequenceNode->children);
		break;
	}

	case BT_NODE_TYPE_DECORATOR:
	{
		btDecoratorNode_t *btDecoratorNode = (btDecoratorNode_t *)btNode;
		NPC_BT_FreeNode(btDecoratorNode->child);
		break;
	}

	default:
		break;
	}

	trap->TrueFree(&btNode);
}

btNodeState_t NPC_RunBehaviorTreeNode(behaviorTree_t *behaviorTree, btNode_t *btNode)
{
	switch (btNode->type)
	{
	case BT_NODE_TYPE_SELECTOR:
	{
		btSelectorNode_t *btSelectorNode = (btSelectorNode_t *)btNode;
		if (btSelectorNode->state != BT_NODE_STATE_RUNNING)
		{
			btSelectorNode->state = BT_NODE_STATE_RUNNING;
			btSelectorNode->activeChildIndex = 0;
		}

		for (;;)
		{
			btNode_t *activeNode = btSelectorNode->children[btSelectorNode->activeChildIndex];
			btNodeState_t nodeState = NPC_RunBehaviorTreeNode(behaviorTree, activeNode);

			if (nodeState != BT_NODE_STATE_FAILED)
			{
				btSelectorNode->state = nodeState;
				return nodeState;
			}

			if (++btSelectorNode->activeChildIndex == btSelectorNode->numChildren)
			{
				btSelectorNode->state = BT_NODE_STATE_FAILED;
				return BT_NODE_STATE_FAILED;
			}
		}
	}

	case BT_NODE_TYPE_SEQUENCE:
	{
		btSequenceNode_t *btSequenceNode = (btSequenceNode_t *)btNode;
		if (btSequenceNode->state != BT_NODE_STATE_RUNNING)
		{
			btSequenceNode->state = BT_NODE_STATE_RUNNING;
			btSequenceNode->activeChildIndex = 0;
		}

		for (;;)
		{
			btNode_t *activeNode = btSequenceNode->children[btSequenceNode->activeChildIndex];
			btNodeState_t nodeState = NPC_RunBehaviorTreeNode(behaviorTree, activeNode);

			if (nodeState != BT_NODE_STATE_SUCCEEDED)
			{
				btSequenceNode->state = nodeState;
				return nodeState;
			}

			if (++btSequenceNode->activeChildIndex == btSequenceNode->numChildren)
			{
				btSequenceNode->state = BT_NODE_STATE_SUCCEEDED;
				return BT_NODE_STATE_SUCCEEDED;
			}
		}
	}

	case BT_NODE_TYPE_DECORATOR:
	{
		btDecoratorNode_t *btDecoratorNode = (btDecoratorNode_t *)btNode;
		if (btDecoratorNode->state != BT_NODE_STATE_RUNNING)
		{
			btDecoratorNode->value = level.time + Q_irand(2000, 5000);
		}

		btDecoratorNode->state = btDecoratorNode->execute(behaviorTree, btNode);
		return btDecoratorNode->state;
	}

	case BT_NODE_TYPE_LEAF:
	{
		btLeafNode_t *btLeafNode = (btLeafNode_t *)btNode;
		if (btLeafNode->state != BT_NODE_STATE_RUNNING)
		{
			btLeafNode->value = level.time;
		}

		btLeafNode->state = btLeafNode->execute(behaviorTree, btNode);
		return btLeafNode->state;
	}

	default:
		assert(!"Invalid node type");
		return BT_NODE_STATE_SUCCEEDED;
	}
}

static btNodeState_t NPC_BT_FindEnemy(behaviorTree_t *bt, btNode_t *node)
{
	int NPC_FindNearestEnemy(gentity_t* ent);
	void G_SpeechEvent(gentity_t *self, int event);

	gentity_t *npc = NPCS.NPC;
	const int enemyEntityNum = NPC_FindNearestEnemy(npc);
	if (enemyEntityNum == -1)
	{
		node->state = BT_NODE_STATE_FAILED;
		npc->enemy = NULL;
	}
	else
	{
		node->state = BT_NODE_STATE_SUCCEEDED;
		npc->enemy = g_entities + enemyEntityNum;
		G_SpeechEvent(npc, Q_irand(EV_DETECTED1, EV_DETECTED5));
	}

	return node->state;
}

static btNodeState_t NPC_BT_FireWeapon(behaviorTree_t *bt, btNode_t *node)
{
	NPCS.ucmd.buttons |= BUTTON_ATTACK;
	node->state = BT_NODE_STATE_SUCCEEDED;
	return node->state;
}

static btNodeState_t NPC_BT_Wait(behaviorTree_t *bt, btNode_t *node)
{
	btLeafNode_t *leafNode = (btLeafNode_t *)node;
	if (leafNode->value > level.time)
	{
		node->state = BT_NODE_STATE_RUNNING;
	}
	else
	{
		node->state = BT_NODE_STATE_SUCCEEDED;
	}

	return node->state;
}

static btNodeState_t NPC_BT_KeepTrying(behaviorTree_t *bt, btNode_t *node)
{
	btDecoratorNode_t *decoratorNode = (btDecoratorNode_t *)node;
	if (decoratorNode->value < level.time)
	{
		return node->state = BT_NODE_STATE_FAILED;
	}
	else
	{
		const btNodeState_t childState = NPC_RunBehaviorTreeNode(bt, decoratorNode->child);
		if (childState == BT_NODE_STATE_SUCCEEDED)
		{
			return node->state == childState;
		}

		return node->state = BT_NODE_STATE_RUNNING;
	}
}

static btNodeState_t NPC_BT_FaceEnemy(behaviorTree_t *bt, btNode_t *node)
{
	qboolean facing = NPC_FaceEnemy(qtrue);
	if (facing)
	{
		node->state = BT_NODE_STATE_SUCCEEDED;
	}
	else
	{
		node->state = BT_NODE_STATE_RUNNING;
	}

	return node->state;
}

static btNodeState_t NPC_BT_HasEnemy(behaviorTree_t *bt, btNode_t *node)
{
	gentity_t *npc = NPCS.NPC;
	if (npc->enemy != NULL && NPC_ValidEnemy(npc->enemy) && G_ClearLOS4(npc, npc->enemy))
	{
		btDecoratorNode_t *decoratorNode = (btDecoratorNode_t *)node;
		node->state = NPC_RunBehaviorTreeNode(bt, decoratorNode->child);
	}
	else
	{
		node->state = BT_NODE_STATE_FAILED;
	}

	return node->state;
}

static btNodeState_t NPC_BT_GetRandomPatrolLocation(behaviorTree_t *bt, btNode_t *node)
{
	gentity_t *npc = NPCS.NPC;

	const int patrolWaypoint = NAV_GetNearestNode(npc, WAYPOINT_NONE);
	if (patrolWaypoint != WAYPOINT_NONE)
	{
		const int numEdges = trap->Nav_GetNodeNumEdges(patrolWaypoint);
		if (numEdges > 0)
		{
			const int edgeWaypoint = trap->Nav_GetNodeEdge(patrolWaypoint, Q_irand(0, numEdges - 1));

			vec3_t nodePosition;
			if (trap->Nav_GetNodePosition(edgeWaypoint, nodePosition))
			{
				const int radius = trap->Nav_GetNodeRadius(edgeWaypoint);

				NPC_SetMoveGoal(NPCS.NPC, nodePosition, radius, qtrue, WAYPOINT_NONE, NULL);
				NPCS.NPC->waypoint = edgeWaypoint;

				return node->state = BT_NODE_STATE_SUCCEEDED;
			}
		}
	}

	NPCS.NPC->waypoint = WAYPOINT_NONE;

	return node->state = BT_NODE_STATE_FAILED;
}

static btNodeState_t NPC_BT_MoveToPatrolLocation(behaviorTree_t *bt, btNode_t *node)
{
	gentity_t *npcEnt = NPCS.NPC;
	gNPC_t *npc = NPCS.NPCInfo;

	if (NAV_HitNavGoal(npcEnt->r.currentOrigin, npcEnt->r.mins, npcEnt->r.maxs, npc->tempGoal->r.currentOrigin, npc->goalRadius, qfalse))
	{
		NPC_ReachedGoal();
		return node->state = BT_NODE_STATE_SUCCEEDED;
	}
	else
	{
		if (!NPC_MoveToGoal(qfalse))
		{
			return node->state = BT_NODE_STATE_FAILED;
		}
		else
		{
			return node->state = BT_NODE_STATE_RUNNING;
		}
	}
}

behaviorTree_t *NPC_CreateBehaviorTree(int entityNum)
{
	behaviorTree_t *bt = s_behaviorTrees + entityNum;

	NPC_FreeBehaviorTree(bt);

	btNode_t *faceEnemyNode = NPC_BT_CreateLeafNode(NPC_BT_FaceEnemy);
	btNode_t *fireNode = NPC_BT_CreateLeafNode(NPC_BT_FireWeapon);
	btNode_t *attackNode = NPC_BT_CreateSequenceNode(2, faceEnemyNode, fireNode);
	btNode_t *attackNodeWithDecorator = NPC_BT_CreateDecoratorNode(NPC_BT_HasEnemy, attackNode);

	btNode_t *getPatrolNode = NPC_BT_CreateLeafNode(NPC_BT_GetRandomPatrolLocation);
	btNode_t *moveToPatrolNode = NPC_BT_CreateLeafNode(NPC_BT_MoveToPatrolLocation);
	btNode_t *findEnemyNode = NPC_BT_CreateLeafNode(NPC_BT_FindEnemy);
	btNode_t *searchNode = NPC_BT_CreateDecoratorNode(NPC_BT_KeepTrying, findEnemyNode);
	btNode_t *patrolNode = NPC_BT_CreateSequenceNode(3, getPatrolNode, moveToPatrolNode, searchNode);

	btNode_t *rootNode = NPC_BT_CreateSelectorNode(2, attackNodeWithDecorator, patrolNode);

	bt->root = rootNode;

	return bt;
}

void NPC_FreeBehaviorTree(behaviorTree_t *bt)
{
	if (bt != NULL && bt->root != NULL)
	{
		NPC_BT_FreeNode(bt->root);
		bt->root = NULL;
	}
}

void NPC_RunBehaviorTree(behaviorTree_t *bt)
{
	NPC_RunBehaviorTreeNode(bt, bt->root); 
}
