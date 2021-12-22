#pragma once

typedef struct behaviorTree_s behaviorTree_t;
typedef struct btNode_s btNode_t;

typedef enum btNodeState_e
{
	BT_NODE_STATE_INVALID,
	BT_NODE_STATE_RUNNING,
	BT_NODE_STATE_FAILED,
	BT_NODE_STATE_SUCCEEDED,
} btNodeState_t;

behaviorTree_t *NPC_CreateBehaviorTree(int entityNum);
void NPC_FreeBehaviorTree(behaviorTree_t *bt);

btNode_t *NPC_BT_CreateSelectorNode(int numChildren, ...);
btNode_t *NPC_BT_CreateSequenceNode(int numChildren, ...);
btNode_t *NPC_BT_CreateDecoratorNode(btNodeState_t(*fn)(behaviorTree_t *, btNode_t *), btNode_t *child);
btNode_t *NPC_BT_CreateLeafNode(btNodeState_t(*fn)(behaviorTree_t *, btNode_t *));

void NPC_RunBehaviorTree(behaviorTree_t *bt);
btNodeState_t NPC_RunBehaviorTreeNode(behaviorTree_t *bt, btNode_t *node);
