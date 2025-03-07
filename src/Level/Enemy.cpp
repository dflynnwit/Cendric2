#include "Level/Enemy.h"
#include "Level/MOBBehavior/MovingBehaviors/EnemyMovingBehavior.h"
#include "Level/MOBBehavior/AttackingBehaviors/EnemyAttackingBehavior.h"
#include "Level/MOBBehavior/AttackingBehaviors/AllyBehavior.h"
#include "Level/MOBBehavior/ScriptedBehavior/ScriptedBehavior.h"
#include "Level/DynamicTiles/LeverTile.h"
#include "Level/Level.h"
#include "Level/LevelMainCharacter.h"
#include "Screens/LevelScreen.h"
#include "ObjectFactory.h"
#include "GameObjectComponents/InteractComponent.h"
#include "Enums/EnumNames.h"
#include "Level/DamageNumbers.h"
#include "GlobalResource.h"

const float Enemy::HP_BAR_HEIGHT = 3.f;
const float Enemy::PICKUP_RANGE = 100.f;

Enemy::Enemy(const Level* level, Screen* screen) : LevelMovableGameObject(level) {
	m_mainChar = dynamic_cast<LevelScreen*>(screen)->getMainCharacter();
	m_screen = screen;
	m_spellManager = new SpellManager(this);

	m_buffBar = new EnemyBuffBar(this);

	m_targetSprite.setTexture(*g_resourceManager->getTexture(GlobalResource::TEX_GUI_CURSOR));
	m_targetSprite.setTextureRect(sf::IntRect(25, 0, 25, 25));
	m_targetSprite.setOrigin(sf::Vector2f(12.f, 12.f));
}

Enemy::~Enemy() {
	delete m_lootWindow;
	delete m_buffBar;
	delete m_scriptedBehavior;
}

void Enemy::load(int skinNr) {
	m_skinNr = skinNr;
	if (m_name.empty())
		m_name = EnumNames::getEnemyName(getEnemyID());

	m_interactComponent = new InteractComponent(g_textProvider->getText(m_name, "enemy"), this, m_mainChar);
	m_interactComponent->setInteractRange(PICKUP_RANGE);
	m_interactComponent->setInteractText("ToLoot");
	m_interactComponent->setOnInteract(std::bind(&Enemy::loot, this));
	m_interactComponent->setInteractable(false);
	m_interactComponent->setTooltipHeight(getConfiguredDistanceToHPBar() - GUIConstants::CHARACTER_SIZE_S);
	addComponent(m_interactComponent);

	loadResources();
	loadAnimation(skinNr);
	loadAttributes();
	loadSpells();
	loadBehavior();
	m_spellManager->setSpellsAllied(m_isAlly);

	delete m_damageNumbers;
	m_damageNumbers = new DamageNumbers(this->isAlly());

	if (!m_spellManager->getSpellMap().empty() && m_movingBehavior != nullptr) {
		const SpellData& spellData = m_spellManager->getSpellMap().at(0)->getSpellData();
		m_movingBehavior->setDefaultFightAnimation(spellData.fightingTime, spellData.fightAnimation);
	}
}

bool Enemy::getFleeCondition() const {
	return false;
}

void Enemy::onHit(Spell* spell) {
	if (m_isDead) {
		if (spell->getSpellID() == SpellID::RaiseTheDead)
			spell->execOnHit(this);
		return;
	}

	if (const LevelMainCharacter* owner = dynamic_cast<const LevelMainCharacter*>(spell->getOwner())) {
		if (!owner->getTargetManager().getCurrentTargetEnemy()) {
			owner->getTargetManager().setTargetEnemy(this);
		}
	}

	LevelMovableGameObject::onHit(spell);
	setChasing();
}

void Enemy::renderAfterForeground(sf::RenderTarget& renderTarget) {
	m_buffBar->render(renderTarget);
	LevelMovableGameObject::renderAfterForeground(renderTarget);
	if (m_isHPBarVisible) {
		renderTarget.draw(m_hpBar);
	}
	if (m_isTargetedEnemy) {
		renderTarget.draw(m_targetSprite);
	}
	if (m_showLootWindow && m_lootWindow != nullptr) {
		m_lootWindow->render(renderTarget);
		m_showLootWindow = false;
	}
}

void Enemy::update(const sf::Time& frameTime) {
	updateEnemyState(frameTime);
	LevelMovableGameObject::update(frameTime);
	if (m_scriptedBehavior != nullptr) {
		if (!m_isDead) {
			m_scriptedBehavior->update(frameTime);
		}
		else {
			m_scriptedBehavior->updateSpeechBubble(frameTime);
		}
	}
	updateHpBar();
	if (m_showLootWindow && m_lootWindow != nullptr) {
		sf::Vector2f pos(getBoundingBox()->left + getBoundingBox()->width, getBoundingBox()->top - m_lootWindow->getSize().y + 10.f);
		m_lootWindow->setPosition(pos);
	}
	m_showLootWindow = m_showLootWindow || g_inputController->isKeyActive(Key::ToggleTooltips);
	m_buffBar->update(frameTime);
	m_targetSprite.setPosition(getBoundingBox()->left + 0.5f * getBoundingBox()->width, getBoundingBox()->top + 0.5f * getBoundingBox()->height);
}

void Enemy::updateHpBar() {
	if (!m_isHPBarVisible) return;
	m_hpBar.setPosition(getBoundingBox()->left, getBoundingBox()->top - getConfiguredDistanceToHPBar());
	m_hpBar.setSize(sf::Vector2f(getBoundingBox()->width * (static_cast<float>(m_attributes.currentHealthPoints) / m_attributes.maxHealthPoints), HP_BAR_HEIGHT));
}

void Enemy::loadBehavior() {
	LevelMovableGameObject::loadBehavior();
	// cast the two behavior components so that we only have to cast them once.
	m_enemyAttackingBehavior = dynamic_cast<EnemyAttackingBehavior*>(m_attackingBehavior);
	m_enemyMovingBehavior = dynamic_cast<EnemyMovingBehavior*>(m_movingBehavior);
	// hp bar fill color is dependent on attacking behavior
	m_hpBar.setFillColor(m_enemyAttackingBehavior ? m_enemyAttackingBehavior->getConfiguredHealthColor() : sf::Color::Yellow);
	updateHpBar();
}

sf::Time Enemy::getConfiguredWaitingTime() const {
	return sf::Time::Zero;
}

sf::Time Enemy::getConfiguredRandomDecisionTime() const {
	int r = rand() % 1500 + 200;
	return sf::milliseconds(r);
}

sf::Time Enemy::getConfiguredFearedTime() const {
	return sf::seconds(6);
}

sf::Time Enemy::getConfiguredChasingTime() const {
	return sf::seconds(1);
}

GameObjectType Enemy::getConfiguredType() const {
	return _Enemy;
}

void Enemy::clearReward() {
	m_reward.lootableGold = 0;
	m_reward.lootableItems.clear();
	m_reward.questConditions.clear();
	m_reward.questTargets.clear();
}

void Enemy::updateEnemyState(const sf::Time& frameTime) {
	// handle dead
	if (m_enemyState == EnemyState::Dead) return;

	// update times
	updateTime(m_waitingTime, frameTime);
	updateTime(m_chasingTime, frameTime);
	updateTime(m_decisionTime, frameTime);

	// handle stunned
	if (m_stunnedTime > sf::Time::Zero) {
		m_enemyState = EnemyState::Stunned;
		return;
	}

	// handle fear
	if (m_fearedTime > sf::Time::Zero) {
		m_enemyState = EnemyState::Fleeing;
		return;
	}

	// handle chasing
	if (m_chasingTime > sf::Time::Zero) {
		m_enemyState = EnemyState::Chasing;
		return;
	}

	// the state must have been chasing a frame before. Wait now.
	if (m_enemyState == EnemyState::Chasing) {
		m_waitingTime = getConfiguredWaitingTime();
	}

	// handle waiting
	if (m_waitingTime > sf::Time::Zero) {
		m_enemyState = EnemyState::Waiting;
	}
	else {
		m_enemyState = EnemyState::Idle;
	}

	if (m_movingBehavior && (m_enemyState == EnemyState::Waiting || m_enemyState == EnemyState::Idle) &&
		m_decisionTime == sf::Time::Zero) {
		// decide again
		m_decisionTime = getConfiguredRandomDecisionTime();
		m_enemyMovingBehavior->makeRandomDecision();
	}
}

bool Enemy::isAlly() const {
	return m_isAlly;
}

bool Enemy::isBoss() const {
	return m_isBoss;
}

bool Enemy::isResurrected() const {
	return m_isResurrected;
}

const LevelMovableGameObject* Enemy::getCurrentTarget() const {
	return m_enemyAttackingBehavior->getCurrentTarget();
}

float Enemy::getConfiguredDistanceToHPBar() const {
	return 20.f;
}

int Enemy::getMentalStrength() const {
	return 0;
}

void Enemy::setAlwaysUpdate() {
	m_isAlwaysUpdate = true;
}

EnemyState Enemy::getEnemyState() const {
	return m_enemyState;
}

const std::string& Enemy::getEnemyName() const {
	return m_name;
}

void Enemy::setWaiting() {
	m_waitingTime = getConfiguredWaitingTime();
	m_enemyState = EnemyState::Waiting;
}

void Enemy::setChasing() {
	m_chasingTime = getConfiguredChasingTime();
	m_enemyState = EnemyState::Chasing;
}

void Enemy::setFleeing() {
	m_fearedTime = getConfiguredFearedTime();
	m_enemyState = EnemyState::Fleeing;
}

void Enemy::setIdle() {
	m_fearedTime = sf::Time::Zero;
	m_chasingTime = sf::Time::Zero;
	m_waitingTime = sf::Time::Zero;
	m_enemyState = EnemyState::Idle;
}

void Enemy::setResurrected() {
	m_isResurrected = true;
}

void Enemy::setLoot(const std::map<std::string, int>& items, int gold) {
	m_reward.lootableItems = items;
	m_reward.lootableGold = gold;
	delete m_lootWindow;
	m_lootWindow = nullptr;
	if (items.empty() && gold <= 0) return;
	m_lootWindow = new LootWindow();
	m_lootWindow->setLoot(items, gold);
}

void Enemy::addQuestTarget(const std::pair<std::string, std::string>& questtarget) {
	m_reward.questTargets.push_back(questtarget);
}

void Enemy::addQuestCondition(const std::pair<std::string, std::string>& questcondition) {
	m_reward.questConditions.push_back(questcondition);
}

void Enemy::setObjectID(int id) {
	m_objectID = id;
}

void Enemy::setEnemyName(const std::string& name) {
	if (name.empty()) return;
	m_name = name;
}

void Enemy::setQuestRelevant(bool relevant) {
	m_isQuestRelevant = relevant;
}

void Enemy::switchLever() {
	LeverTile* nearest = nullptr;
	float nearestDist = 10000.f;
	for (auto go : *m_screen->getObjects(_DynamicTile)) {
		auto tile = dynamic_cast<LeverTile*>(go);
		if (!tile) continue;
		auto distance = dist(tile->getCenter(), getCenter());
		if (distance < nearestDist) {
			nearestDist = distance;
			nearest = tile;
		}
	}

	if (nearest) {
		nearest->switchLever();
	}
}

void Enemy::setAlly(const sf::Time& ttl) {
	m_isAlly = true;
	delete m_movingBehavior;
	m_movingBehavior = createMovingBehavior(true);
	m_enemyMovingBehavior = dynamic_cast<EnemyMovingBehavior*>(m_movingBehavior);

	delete m_attackingBehavior;
	m_attackingBehavior = createAttackingBehavior(true);
	AllyBehavior* allyBehavior = dynamic_cast<AllyBehavior*>(m_attackingBehavior);

	if (allyBehavior == nullptr) {
		g_logger->logError("Enemy::setAlly", "Enemy can't be risen as an ally, no AllyBehavior or NeutralBehavior created.");
		return;
	}

	allyBehavior->setTimeToLive(ttl);
	m_enemyAttackingBehavior = allyBehavior;
	m_hpBar.setFillColor(m_enemyAttackingBehavior->getConfiguredHealthColor());
	m_isAlwaysUpdate = true;

	m_spellManager->setSpellsAllied(true);

	setDebugBoundingBox(COLOR_GOOD);
}

void Enemy::setUnique(bool value) {
	m_isUnique = value;
}

void Enemy::setFeared(const sf::Time& fearedTime) {
	if (m_isDead) return;
	LevelMovableGameObject::setFeared(fearedTime);
	m_buffBar->addFeared(fearedTime);
}

void Enemy::setStunned(const sf::Time& stunnedTime) {
	if (m_isDead) return;
	LevelMovableGameObject::setStunned(stunnedTime);
	m_buffBar->addStunned(stunnedTime);
}

void Enemy::addDamageOverTime(DamageOverTimeData& data) {
	if (m_isDead || data.damageType == DamageType::VOID || data.damage <= 0) return;
	m_buffBar->addDotBuff(data.duration, data.damageType);
	LevelMovableGameObject::addDamageOverTime(data);
}

void Enemy::clearDots() {
	LevelMovableGameObject::clearDots();
	m_buffBar->clear();
}

void Enemy::onMouseOver() {
	if (m_isLooted) return;
	LevelMovableGameObject::onMouseOver();
	if (m_isDead && !isAlly()) {
		setSpriteColor(COLOR_INTERACTIVE, sf::milliseconds(100));
		m_showLootWindow = true;
	}
}

void Enemy::loot() {
	if (m_isLooted) return;
	m_mainChar->lootItems(m_reward.lootableItems);
	m_mainChar->addGold(m_reward.lootableGold);
	notifyLooted();
	setDisposed();
}

void Enemy::onRightClick() {
	if (!m_isDead && !isAlly() && m_isHPBarVisible) {
		m_mainChar->getTargetManager().setTargetEnemy(this);
		g_inputController->lockAction();
	}

	if (!m_interactComponent->isInteractable()) return;
	if (m_isDead && !isAlly()) {
		// check if the enemy body is in range
		if (dist(m_mainChar->getCenter(), getCenter()) <= PICKUP_RANGE) {
			loot();
		}
		else {
			m_screen->setNegativeTooltip("OutOfRange");
		}
	}
}

void Enemy::setScriptedBehavior(const std::string& luaPath) {
	delete m_scriptedBehavior;
	m_scriptedBehavior = new ScriptedBehavior(luaPath, this);
	if (m_scriptedBehavior->isError()) {
		delete m_scriptedBehavior;
		m_scriptedBehavior = nullptr;
	}
	else {
		m_isAlwaysUpdate = true;
	}
}

void Enemy::setDisposed() {
	LevelMovableGameObject::setDisposed();
	clearSpells(true);
	if (m_isTargetedEnemy) {
		m_mainChar->getTargetManager().setTargetEnemy(nullptr);
	}
}

void Enemy::setDead() {
	if (m_isDead || m_isImmortal || m_mainChar->isDead()) return;
	LevelMovableGameObject::setDead();
	m_buffBar->clear();

	if (m_isTargetedEnemy) {
		m_mainChar->getTargetManager().setTargetEnemy(nullptr);
	}

	if (m_scriptedBehavior != nullptr) {
		m_scriptedBehavior->onDeath();
	}

	if (isAlly()) {
		m_isLooted = true;
		m_interactComponent->setInteractable(false);
		return;
	}

	if (m_isUnique && !m_isBoss) {
		notifyKilled();
	}

	if (m_isBoss) {
		m_isLooted = true;
		m_interactComponent->setInteractable(false);

		dynamic_cast<LevelScreen*>(m_screen)->notifyBossKilled(m_reward);
	}
	else {
		m_interactComponent->setInteractable(true);
	}
}

void Enemy::notifyLooted() {
	if (m_objectID >= 0) {
		m_screen->getCharacterCore()->setEnemyLooted(m_mainChar->getLevel()->getID(), m_objectID);
	}
	m_isLooted = true;
}

void Enemy::notifyKilled() {
	if (m_screen->getCharacterCore()->isEnemyKilled(m_mainChar->getLevel()->getID(), m_objectID)) return;
	m_screen->getCharacterCore()->setEnemyKilled(m_mainChar->getLevel()->getID(), m_objectID);
	for (auto& target : m_reward.questTargets) {
		dynamic_cast<LevelScreen*>(m_screen)->notifyQuestTargetKilled(target.first, target.second);
	}
	for (auto& condition : m_reward.questConditions) {
		dynamic_cast<LevelScreen*>(m_screen)->notifyQuestConditionFulfilled(condition.first, condition.second);
	}
}

bool Enemy::isQuestRelevant() {
	if (m_isQuestRelevant) {
		return true;
	}
	if (!m_reward.questConditions.empty() && !m_isDead) {
		return true;
	}
	if (!m_reward.questTargets.empty() && !m_isDead) {
		return true;
	}
	if (m_isLooted) return false;
	for (auto& it : m_reward.lootableItems) {
		if (Item::isQuestRelevant(it.first) || dynamic_cast<LevelScreen*>(m_screen)->isItemMonitored(it.first)) {
			return true;
		}
	}
	return false;
}

void Enemy::setMovingTarget(int x, int y) {
	m_enemyMovingBehavior->setMovingTarget(x, y);
}

void Enemy::resetMovingTarget() {
	m_enemyMovingBehavior->resetMovingTarget();
}

void Enemy::setTargeted(bool targeted) {
	m_isTargetedEnemy = targeted;
}

int Enemy::getSkinNr() const {
	return m_skinNr;
}