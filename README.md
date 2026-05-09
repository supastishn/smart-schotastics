# Smart Stocchastics

Smart Stochastics is a (planned) library that includes many different stochastic search implementations (a stochastic search is a search algorithm that uses randomness to find mostly optimal solutions).

The library includes: Simulated Annealing + descent into local minima: after temperature drops below a certain point, it evaluates f(x+h) and f(x-h) for all arguments with small h then descend into wherever it found.

Star feature: Multi-surrogate Bayesian

Instead of just one surrogate you have a vector<SurrogateModel>. Then you can insert multiple surrogates into the vector and then the algorithm will use all of them to evaluate the next point. It works like a council: consensus or majority of surrogates on a point means it has a high likelihood of being predicted correctly.

Another feature of the multi-surrogate Bayesian is using Simulated Annealing with it. However:

The first anneal runs on the real f(x). All points are used to train surrogate models.

This repeats, until the surrogate models have reached  acritical accuracy. Then, the next anneals/acquisitions use the surrogate model exclusively in order to find the nost likely global minima via S.A.

Each simulated annealing leaves a "breadcrumbs" of visited points.

After that, it checks on the final point with the real f(x) to see if it is indeed a likrly global minima. If it is, then it adds it to best solutions.

If it is wrong, then you binary search throuth the breadcrumbs to find an early point where the surrlgate models went wrong. While it is not guaranteed to be monotonic, our goal is only to find a few points where the surrogate models are wrong, so we can add those points to the training set and retrain the surrogate models. And it is highly likely that the more we push into a region where predictions are inaccurate, the more wrong the predictikns are.

Each surrogate model gets a percentage of the vote based off their accuracy on a validation set of the last few points. This way, if a surrogate model is very inaccurate, it will have a very low percentage of the vote and thus will not influence the next point as much. On the other hand, if a surrogate model is very accurate, it will have a high percentage of the vote and thus will influence the next predictions more.

I may decide to include other stochastic search algorithms such as genetic algorithms, particle swarm optimization, etc. in the future, but for now I will focus on Simulated Annealing and the multi-surrogate Bayesian approach.
