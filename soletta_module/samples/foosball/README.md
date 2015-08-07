# Foosball samples

There are two foosballs samples using soletta in this directory.
Sample 'foosball' runs only using gtk and 'foolsball_limited' runs using gtk,
Galileo gen 1 and Edison.

## Foosball Sample

 * Running with Fuzzy Engine

   $ SOL_FLOW_MODULE_RESOLVER_CONFFILE=gtk_fuzzy.json ./foosball.fbp

 * Running with Neural Network Engine

   $ SOL_FLOW_MODULE_RESOLVER_CONFFILE=gtk_ann.json ./foosball.fbp

## Foosball Limited Sample

This example has limited IO, so it is easier to adapt it  to run it in galileo
or edison boards.

 * Running using gtk
   $ SOL_FLOW_MODULE_RESOLVER_CONFFILE=gtk_fuzzy.json ./foosball_limited.fbp

   or

   $ SOL_FLOW_MODULE_RESOLVER_CONFFILE=gtk_ann.json ./foosball_limited.fbp

 * Running in Galileo Gen 1 Board or Edison

This example was tested in Galileo Gen 1 and Edison boards using grove-kit. If
you want to test it in other boards or using other IO configuration, it is
necessary to update pins in json conf files.

For this example, place main button in D4, next button in D3, Led1 in D5 and
Led2 in D6. Player is selected using main button and next button moves the
selection to next player. As soon as all players are selected, the game starts.
and main button is used to score goals for team1 and next button is used to
score goals to team2. At the end of the game, sample will go back to player
selection.

During the game, Led1 and Led2 are used to show sml predicts that the winner
will be respectively team1 or team2. Player selection and score information
are printed in console.

 * Running in Galileo Gen 1
   $ SOL_FLOW_MODULE_RESOLVER_CONFFILE=galileo_fuzzy.json ./foosball_limited.fbp

   or

   $ SOL_FLOW_MODULE_RESOLVER_CONFFILE=galileo_ann.json ./foosball_limited.fbp

 * Running in Edison
   $ SOL_FLOW_MODULE_RESOLVER_CONFFILE=edison_fuzzy.json ./foosball_limited.fbp

   or

   $ SOL_FLOW_MODULE_RESOLVER_CONFFILE=edison_ann.json ./foosball_limited.fbp

