// csx2dif.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "tinyxml2.h"
#include "glm\glm.hpp"
#include "glm\gtx\transform.hpp"
#include <vector>
#include "DIFBuilder\DIFBuilder.hpp"

glm::mat4x4 parseMatrix(const char* str)
{
	float a, b, c, d;
	float e, f, g, h;
	float i, j, k, l;
	float m, n, o, p;
	sscanf(str, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", &a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p);
	return glm::mat4x4(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
}

glm::vec3 parseVec(const char* str)
{
	float x, y, z;
	sscanf(str, "%f %f %f", &x, &y, &z);
	return glm::vec3(x, y, z);
}

glm::mat2x4 parseTexgenPlanes(const char* str)
{
	float x1, y1, z1, d1, x2, y2, z2, d2;
	sscanf(str, "%f %f %f %f %f %f %f %f", &x1, &y1, &z1, &d1, &x2, &y2, &z2, &d2); //TODO: rotation and scaling values
	return glm::mat2x4(x1, y1, z1, d1, x2, y2, z2, d2);
}

std::vector<int> parseIndices(const char* str)
{
	std::vector<int> tempIndices;

	char* strs = (char*)malloc(strlen(str) + 1);
	strcpy(strs, str);

	char* tokn = strtok(strs, " ");

	while (tokn != NULL)
	{
		tempIndices.push_back(atoi(tokn));
		tokn = strtok(NULL," ");
	}
	
	std::vector<int> triangulatedIndices;
	
	for (int i = 0; i < tempIndices.size() - 2; i++)
	{
		triangulatedIndices.push_back(tempIndices[0]);
		triangulatedIndices.push_back(tempIndices[i + 1]);
		triangulatedIndices.push_back(tempIndices[i + 2]);
	}

	return triangulatedIndices;
}

int main(int argc, const char** argv)
{
	printf("csx2dif 1.0\n");

	tinyxml2::XMLDocument csx;

	FILE *f;
	f = fopen(argv[1], "rb");

	DIF::DIFBuilder builder;

	std::map<int, DIF::DIFBuilder*> mpbuilders;

	//We load the csx file using the xml parser
	printf("Loading CSX\n");
	int loadcode = csx.LoadFile(f);
	if (loadcode == tinyxml2::XML_SUCCESS)
	{
		//Navigate our way to the xml node we want
		printf("Parsing CSX\n");
		tinyxml2::XMLElement* cscene = csx.FirstChildElement("ConstructorScene");
		tinyxml2::XMLElement* detail = cscene->FirstChildElement("DetailLevels")->FirstChildElement("DetailLevel");
		tinyxml2::XMLElement* interiormap = detail->FirstChildElement("InteriorMap");

		//Parse entities
		tinyxml2::XMLElement* entities = interiormap->FirstChildElement("Entities");
		tinyxml2::XMLElement* entity = entities->FirstChildElement();

		//This is for looking up what the entity is from its id, used for MPs and path_nodes(constructor doesnt do pathnodes like this but I added it as a new feature)
		std::map<int, tinyxml2::XMLElement*> grouplookup;

		//Iterate over all entities
		while (entity != NULL)
		{
			const char* classname = entity->Attribute("classname");
			if (strcmp(classname, "worldspawn") == 0) //We arent exporting this
			{
				entity = entity->NextSiblingElement();
				continue;
			}
			if (strcmp(classname, "Door_Elevator") == 0 || strcmp(classname, "path_node") == 0) 
			{
				//We put this entity in the group lookup, would be needed later on
				grouplookup.insert(std::pair<int, tinyxml2::XMLElement*>(atoi(entity->Attribute("id")), entity));
				entity = entity->NextSiblingElement();
				continue;
			}

			//Parse the GameEntities
			DIF::GameEntity* ge = new DIF::GameEntity();
			ge->position = parseVec(entity->Attribute("origin"));
			ge->datablock = classname;
			
			tinyxml2::XMLElement* properties = entity->FirstChildElement("Properties");
			const tinyxml2::XMLAttribute* prop = properties->FirstAttribute();

			while (prop != NULL)
			{
				ge->properties.push_back(std::pair<std::string, std::string>(std::string(prop->Name()), std::string(prop->Value())));
				prop = prop->Next();
			}

			ge->gameClass = std::string(properties->Attribute("game_class"));

			builder.addEntity(*ge);

			entity = entity->NextSiblingElement();
				
		}

		//Parse Brushes
		tinyxml2::XMLElement* brushes = interiormap->FirstChildElement("Brushes");
		tinyxml2::XMLElement* brush = brushes->FirstChildElement();

		while (brush != NULL)
		{

			int owner = brush->IntAttribute("owner");
			int type = brush->IntAttribute("type");

			bool isMP = false;
			if (owner != 0 && type == 999)
				isMP = true;

			const char* transformattrib = brush->Attribute("transform");

			glm::mat4x4 transform = parseMatrix(transformattrib);

			std::vector<glm::vec3> vertices;

			tinyxml2::XMLElement* verticesElement = brush->FirstChildElement("Vertices");
			tinyxml2::XMLElement* vertex = verticesElement->FirstChildElement();

			while (vertex != NULL)
			{
				glm::vec3 vertexvec = parseVec(vertex->Attribute("pos"));

				vertices.push_back(glm::vec3(transform * glm::vec4(vertexvec,0)));

				vertex = vertex->NextSiblingElement();
			}

			tinyxml2::XMLElement* face = brush->FirstChildElement("Face");

			while (face != NULL)
			{
				//parseIndices just triangle-fans all the indices internally
				std::vector<int> indices = parseIndices(face->FirstChildElement("Indices")->Attribute("indices"));

				glm::mat2x4 texgenplanes = parseTexgenPlanes(face->Attribute("texgens"));

				for (int i = 0; i < indices.size(); i+=3)
				{
					glm::vec3 p1 = vertices[indices[i]];
					glm::vec3 p2 = vertices[indices[i+1]];
					glm::vec3 p3 = vertices[indices[i+2]];

					glm::vec2 uv1 = glm::vec2(p1.x * texgenplanes[0].x + p1.y * texgenplanes[0].y + p1.z * texgenplanes[0].z + texgenplanes[0].w, p1.x * texgenplanes[1].x + p1.y * texgenplanes[1].y + p1.z * texgenplanes[1].z + texgenplanes[1].w);
					glm::vec2 uv2 = glm::vec2(p2.x * texgenplanes[0].x + p2.y * texgenplanes[0].y + p2.z * texgenplanes[0].z + texgenplanes[0].w, p2.x * texgenplanes[1].x + p2.y * texgenplanes[1].y + p2.z * texgenplanes[1].z + texgenplanes[1].w);
					glm::vec2 uv3 = glm::vec2(p3.x * texgenplanes[0].x + p3.y * texgenplanes[0].y + p3.z * texgenplanes[0].z + texgenplanes[0].w, p3.x * texgenplanes[1].x + p3.y * texgenplanes[1].y + p3.z * texgenplanes[1].z + texgenplanes[1].w);

					DIF::DIFBuilder::Point pt1 = DIF::DIFBuilder::Point(p1, uv1);
					DIF::DIFBuilder::Point pt2 = DIF::DIFBuilder::Point(p2, uv2);
					DIF::DIFBuilder::Point pt3 = DIF::DIFBuilder::Point(p3, uv3);

					//We need to build separate interiors too if there are any MPs
					if (isMP)
					{
						auto mpbuilderptr = mpbuilders.find(owner);
						if (mpbuilderptr == mpbuilders.end())
						{
							DIF::DIFBuilder *db = new DIF::DIFBuilder();
							mpbuilders.insert(std::pair<int, DIF::DIFBuilder*>(owner, db));
							db->addTriangle(DIF::DIFBuilder::Triangle(pt1, pt2, pt3), face->Attribute("material"));
						}
						else
							(*mpbuilderptr).second->addTriangle(DIF::DIFBuilder::Triangle(pt1, pt2, pt3), face->Attribute("material"));
					}
					else
					builder.addTriangle(DIF::DIFBuilder::Triangle(pt1, pt2, pt3), face->Attribute("material"));
				}

				face = face->NextSiblingElement("Face");
			}

			brush = brush->NextSiblingElement();
		}

		printf("Building pathedInteriors if any exist\n");
		//Now build MPs if there are any
		for (auto& mpbuilder : mpbuilders)
		{
			DIF::DIF d;
			mpbuilder.second->build(d);

			tinyxml2::XMLElement* entity = grouplookup[mpbuilder.first]; //mpbuilder.first is actually the index of the Door_Elevator entity
			std::vector<tinyxml2::XMLElement*> pathnodes;
			for (auto& entity2 : grouplookup) //We collect all the pathnodes which are for this MP, new feature.
				if (entity2.second->IntAttribute("owner") == mpbuilder.first)
					pathnodes.push_back(entity2.second);

			if (pathnodes.size() == 0) //Well, we cant find any, so just put all of MPs like constructor does
			{
				for (auto& entity2 : grouplookup)
				{
					if (strcmp(entity2.second->Attribute("classname"), "path_node") == 0)
						pathnodes.push_back(entity2.second);
				}
			}
			//Parse pathnodes
			std::vector<DIF::Marker> markers;
			for (auto& pathnode : pathnodes)
			{
				DIF::Marker m;
				m.initialTargetPosition = entity->FirstChildElement("Properties")->IntAttribute("InitialTargetPosition", -1);
				m.initialPathPosition = entity->FirstChildElement("Properties")->IntAttribute("InitialPathPosition", 0);
				m.msToNext = pathnode->FirstChildElement("Properties")->IntAttribute("next_time");
				m.position = parseVec(pathnode->Attribute("origin"));
				m.smoothing = pathnode->FirstChildElement("Properties")->IntAttribute("smoothing");
				
				markers.push_back(m);
			}

			builder.addPathedInterior(d.interior[0], markers);
			
		}

		//Finally build the DIF and write it
		printf("Starting conversion\n");
		DIF::DIF finaldif;
		builder.build(finaldif);
		
		std::string fname = std::string(argv[1]);
		fname = fname.substr(0, fname.length() - 3) + "dif";

		printf("Writing DIF file\n");
		std::ofstream outStr;
		outStr.open(std::string(argv[1]).substr(0, strlen(argv[1]) - 3) + "dif", std::ios::out | std::ios::binary);
		finaldif.write(outStr, DIF::Version());
	}
	else
	{
		printf("%s", csx.ErrorIDToName((tinyxml2::XMLError)loadcode));
	}
}
