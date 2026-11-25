# Virtual Memory Manager

Projeto desenvolvido em C para a disciplina de Infraestrutura
de Software da CESAR School.

O programa simula um gerenciador de memória virtual com
128 frames e uma TLB de 16 entradas, utilizando os algoritmos
FIFO e LRU. A busca na TLB é feita com threads.

## Como executar

Para compilar:

    make

Para executar:

    ./vm address.txt lru fifo

O segundo argumento define o algoritmo de substituição
de páginas e o terceiro define o da TLB.

O arquivo BACKING_STORE.bin deve estar na pasta de execução.

## Testes

    ./test_vm.sh

As quatro combinações de algoritmos foram testadas com
1000 endereços, sem erros nos valores recuperados.

## Limpeza

    make clean

