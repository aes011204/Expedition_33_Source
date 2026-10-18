#include "AStar.h"
//Funnel , line of Sight, 휴리스틱 , 리셈플링 해야함 
AStar::AStar()
{
}

AStar::~AStar()
{
}

vector<Vec3> AStar::FindPath(Vec3 start, Vec3 end)
{
    // priority_queue<ANode, vector<ANode>, Compare> _open; // 갈수 있는 길
    // unordered_map<int, ANode> _allNode;
    // unordered_set<int> _closed; // 이미 본 아이디인지 확인용 // 이미 거쳐온 곳 


    //// _open.push()
    // int start = nav->FindClosestPoly(startPos);
    // int goal = nav->FindClosestPoly(endPos);

    // ANode starNode;
    // starNode.id = start;
    // starNode.h = (nav->GetPoly(start).center - nav->GetPoly(goal).center).distance();
    // starNode.g = 0.f;
    // starNode.f = starNode.g + starNode.h;
    // starNode.parent = -1;

    // // 
    // _open.push(starNode);
    // _allNode[start] = starNode;

    // while (!_open.empty())
    // {
    //     ANode current = _open.top();
    //     _open.pop();

    //     if (current.id == goal)
    //         return Recalculate(_allNode,goal);

    //     if (_closed.contains(current.id))
    //         continue;

    //     if (_allNode[current.id].g < current.g)
    //         continue;

    //     _closed.insert(current.id);

    //     for (auto& neighborID : nav->GetPoly(current.id).neighbors)
    //     {
    //         if (_closed.contains(neighborID))
    //             continue;

    //         float newG = current.g +
    //             (nav->GetPoly(current.id).center - nav->GetPoly(neighborID).center).distnace();

    //         if (!_allNode.contains(neighborID) || newG < _allNode[neighborID].g)
    //         {
    //             ANode nextNode;
    //             nextNode.id = neighborID;
    //             nextNode.h = (nav->GetPoly(neighborID).center - nav->GetPoly(goal).center).distance();
    //             nextNode.g = newG;
    //             nextNode.f = nextNode.g + nextNode.h;
    //             nextNode.parent = current.id;

    //             _allNode[neighborID] = nextNode;
    //             _open.push(nextNode);
    //         }
    //     }

    // }


    return vector<Vec3>();
}

vector<Vec3> AStar::Recalculate(unordered_map<int, ANode>& allNode, int goal)
{
    vector<Vec3> path;

    //int current = goal;
    //
    //while (current != -1)
    //{
    //    path.push_back(nav->GetPoly(current).center);
    //    current = allNode[current].parent;
    //}
    //
    //reverse(path.begin(), path.end());

    return path;
}
