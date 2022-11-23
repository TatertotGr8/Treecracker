# Tree Cracker 

This program will return possible world seeds entirely based on data from trees in Minecraft. Naturally, a task like this would have some limitations this program is quite time-consuming to run as is limited to "Text Seeds". To summarize seeds in Minecraft are 64 bits (2^64 possible unique seeds) but a 32-bit integer hash is used to generate seeds containing text. Meaning text seeds have 2^32 possible seeds MUCH less than 2^64. In theory, this program *could* find whole 64-bit world seeds but that is EXTREMELY impractical.   

This project is heavily inspired by work by the Minecraft@home community. For the unfamiliar, the Minecraft@home community specializes in project-based technical Minecraft projects, from seed cracking to building computers in Minecraft. The community is credited for a lot of the large-scale Minecraft seed reversals specifically [the finding of the title screen panoramas](https://minecraftathome.com/minecrafthome/projects/1-13-1-16-panoramas.html) or the [Pack.png project](https://minecraftathome.com/minecrafthome/projects/packpng.html). To me, it was incredible that from a _random_  Minecraft screenshot the exact world could be found. Many aspects went into these     



## Program Use 
This project requires you to have a few things installed: 

- [The Cuda Toolkit](https://developer.nvidia.com/cuda-toolkit)

- [Clion](https://www.jetbrains.com/clion/)

 

Clone this repository into Clion: 

```bash 
 https://github.com/TatertotGr8/Treecracker.git

```

build the project and then run it. 
Screen recording found [here](https://youtu.be/99p3n8MBqj0) 


Changing the program to search for a different world seed requires more knowledge and understanding related to Minecraft seed cracking, information found in the written documentation [HERE](https://docs.google.com/document/d/1S-tqtsDtqdalQDEEsopy5CnU4O1-bL9xtSGgOIrrxzI/edit#)




## Resources 
  
 - [MCRcortex](https://github.com/MCRcortex)

 - [Minecraft@home](https://minecraftathome.com/)
 
 - [CUDA](https://docs.nvidia.com/cuda/) 
 
 - [Clion](https://www.jetbrains.com/clion/)


## Support

For support or questions about technical Minecraft, email tg24006@pathwayshigh.org or contact Tatertot#7962 on discord.
